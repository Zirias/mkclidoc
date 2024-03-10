#include "manwriter.h"

#include "clidoc.h"
#include "htmlhdr.h"
#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum Fmt
{
    F_MAN,
    F_MDOC,
    F_HTML
} Fmt;

typedef struct FmtOpts
{
    union {
	/* FMT_MDOC */
	int os;

	/* FMT_HTML */
	struct {
	    const char *style;
	    const char *styleuri;
	};
    };
} FmtOpts;

typedef struct Ctx
{
    const FmtOpts *opts;
    const CliDoc *root;
    const char *name;
    const char *arg;
    size_t nflags;
    size_t nargs;
    int separators;
    Fmt fmt;
} Ctx;
#define Ctx_init(root, mdoc, opts) { \
    (opts), (root), 0, 0, 0, 0, 0, (fmt)}

#define err(m) do { \
    fprintf(stderr, "Cannot write man: %s\n", (m)); goto error; } while (0)
#define istext(m) ((m) && CliDoc_type(m) == CT_TEXT)
#define ismpunct(c) (ispunct(c) && (c) != '\\' && (c) != '%' \
	&& (c) != '`' && (c) != '<' && (c) != '>')
#define istpunct(s) (ismpunct(*(s)) && \
	(!(s)[1] || (s)[1] == ' ' || (s)[1] == '\t'))

static char strbuf[8192];
static char escbuf[8192];

static char *strToUpper(const char *str)
{
    size_t len = strlen(str);
    if (len + 1 > sizeof strbuf) len = sizeof strbuf - 1;
    for (size_t i = 0; i < len; ++i)
    {
	strbuf[i] = toupper(str[i]);
    }
    strbuf[len] = 0;
    return strbuf;
}

static char *htmlnescape(const char *str, size_t n)
{
    size_t outlen = 0;
    size_t inpos = 0;
    while (str[inpos] && (!n || inpos < n) &&  outlen < 8150)
    {
	switch (str[inpos])
	{
	    case '<':
		strcpy(escbuf+outlen, "&lt;");
		outlen += 4;
		++inpos;
		break;
	    case '>':
		strcpy(escbuf+outlen, "&gt;");
		outlen += 4;
		++inpos;
		break;
	    case '&':
		strcpy(escbuf+outlen, "&amp;");
		outlen += 5;
		++inpos;
		break;
	    case '"':
		strcpy(escbuf+outlen, "&dquot;");
		outlen += 7;
		++inpos;
		break;
	    default:
		escbuf[outlen++] = str[inpos++];
	}
    }
    escbuf[outlen] = 0;
    return escbuf;
}

static char *htmlescape(const char *str)
{
    return htmlnescape(str, 0);
}

static char *fetchManTextWord(const char **s, const Ctx *ctx)
{
    size_t wordlen = 0;
    while (**s && **s != ' ' && **s != '\t'
	    && !istpunct(*s) && wordlen < 4096)
    {
	if (**s == '%')
	{
	    if (!strcmp(*s, "%%name%%"))
	    {
		if (wordlen) break;
		strcpy(strbuf, "%%name%%");
		*s += sizeof "%%name%%" - 1;
		return strbuf;
	    }
	    if (!strcmp(*s, "%%arg%%"))
	    {
		if (wordlen) break;
		strcpy(strbuf, "%%arg%%");
		*s += sizeof "%%arg%%" - 1;
		return strbuf;
	    }
	}
	if (ctx->fmt != F_HTML && **s == '\\') strbuf[wordlen++] = '\\';
	strbuf[wordlen++] = *(*s)++;
    }
    if (!wordlen) return 0;
    strbuf[wordlen] = 0;
    return strbuf;
}

static void writeManText(FILE *out, Ctx *ctx, const char *str)
{
    size_t col = 0;
    int oneword = 0;
    int nl = 0;
    while (*str)
    {
	int space = 0;
	while (*str && (*str == ' ' || *str == '\t')) ++str, space = 1;
	if (!*str) break;

	if (ctx->fmt != F_HTML && *str == '.' &&
		(!str[1] || str[1] == ' ' || str[1] == '\t'))
	{
	    if (nl) fputc(' ', out);
	    fputc(*str++, out);
	    nl = 1;
	    continue;
	}
	if (ismpunct(*str))
	{
	    if (nl)
	    {
		fputc('\n', out);
		col = 0;
		nl = 0;
	    }
	    if ((col || ctx->fmt == F_HTML) && space) fputc(' ', out), ++col;
	    size_t punctlen = 1;
	    while (ismpunct(str[punctlen]) && str[punctlen] != '.') ++punctlen;
	    if (ctx->fmt != F_HTML && col && col + punctlen > 78)
	    {
		fputc('\n', out);
		col = 0;
	    }
	    fwrite(str, 1, punctlen, out);
	    str += punctlen;
	    col += punctlen;
	    if (oneword)
	    {
		oneword = 0;
		col = 80;
	    }
	    continue;
	}

	char *word = fetchManTextWord(&str, ctx);
	if (!word) break;
	int writename = !strcmp(word, "%%name%%");
	int writearg = !strcmp(word, "%%arg%%") && ctx->arg;
	if (writename || writearg)
	{
	    if (col) fputc('\n', out);
	    if (writename)
	    {
		if (ctx->fmt == F_HTML)
		{
		    fprintf(out, "<span class=\"name\">%s</span>", ctx->name);
		}
		else if (ctx->fmt == F_MDOC) fputs(".Nm", out);
		else fprintf(out, "\\fB%s\\fR", ctx->name);
	    }
	    else if (writearg)
	    {
		if (ctx->fmt == F_HTML)
		{
		    fprintf(out, "<span class=\"arg\">%s</span>", ctx->arg);
		}
		else fprintf(out, ctx->fmt == F_MDOC
			? ".Ar %s" : "\\fI%s\\fR", ctx->arg);
	    }
	    if (ctx->fmt == F_MDOC)
	    {
		if (ismpunct(*str))
		{
		    fputc(' ', out);
		    oneword = 1;
		}
		else if (*str == ' ' || *str == '\t') nl = 1;
		else if (*str)
		{
		    fputs(" Ns ", out);
		    oneword = 1;
		}
	    }
	    else if (ctx->fmt == F_MAN)
	    {
		if (*str == ' ' || *str == '\t') nl = 1;
		else if (*str) oneword = 1;
	    }
	    else if (*str == ' ' || *str == '\t') fputc('\n', out);
	    continue;
	}
	size_t wordlen = strlen(word);
	if (word[0] == '`' && word[wordlen-1] == '`')
	{
	    const CliDoc *ref = 0;
	    for (size_t i = 0; i < CDRoot_nrefs(ctx->root); ++i)
	    {
		const CliDoc *r = CDRoot_ref(ctx->root, i);
		if (!strncmp(CDMRef_name(r), word+1, wordlen-2))
		{
		    ref = r;
		    break;
		}
	    }
	    if (ref)
	    {
		if (col) fputc('\n', out);
		if (ctx->fmt == F_HTML)
		{
		    fprintf(out, "<span class=\"name\">%s</span>(%s)",
			    CDMRef_name(ref), CDMRef_section(ref));
		}
		else fprintf(out, ctx->fmt == F_MDOC
			? ".Xr %s %s" : "\\fB%s\\fP(%s)\\fR",
			CDMRef_name(ref), CDMRef_section(ref));
		if (ctx->fmt == F_MDOC)
		{
		    if (ismpunct(*str))
		    {
			fputc(' ', out);
			oneword = 1;
		    }
		    else if (*str == ' ' || *str == '\t') nl = 1;
		    else if (*str)
		    {
			fputs(" Ns ", out);
			oneword = 1;
		    }
		}
		else if (ctx->fmt == F_MAN)
		{
		    if (*str == ' ' || *str == '\t') nl = 1;
		    else if (*str) oneword = 1;
		}
		else if (*str == ' ' || *str == '\t') fputc('\n', out);
		continue;
	    }
	}
	else if (word[0] == '<' && word[wordlen-1] == '>')
	{
	    int islink = !!strstr(word+1, "://");
	    int isemail = !islink && !!strchr(word+1, '@');
	    if (islink || isemail)
	    {
		word[wordlen-1] = 0;
		++word;
		if (col) fputc('\n', out);
		if (ctx->fmt == F_HTML)
		{
		    char *email = htmlescape(word);
		    fprintf(out, isemail ? "<a href=\"mailto:%s\">%s</a>"
			    : "<a href=\"%s\">%s</a>",
			    email, email);
		}
		else fprintf(out, ctx->fmt == F_MDOC
			? ( isemail ? ".Aq Mt %s" : ".Lk %s" )
			: ( isemail ? "<\\fI%s\\fR>" : "\\fB%s\\fR" ), word);
		if (ctx->fmt == F_MDOC)
		{
		    if (ismpunct(*str))
		    {
			fputc(' ', out);
			oneword = 1;
		    }
		    else if (*str == ' ' || *str == '\t') nl = 1;
		    else if (*str)
		    {
			fputs(" Ns ", out);
			oneword = 1;
		    }
		}
		else if (ctx->fmt == F_MAN)
		{
		    if (*str == ' ' || *str == '\t') nl = 1;
		    else if (*str) oneword = 1;
		}
		else if (*str == ' ' || *str == '\t') fputc('\n', out);
		continue;
	    }
	}
	if (nl || (ctx->fmt != F_HTML && col + !!col + wordlen > 78))
	{
	    fputc('\n', out);
	    col = 0;
	    nl = 0;
	}
	if (col && space) ++col, fputc(' ', out);
	if (ctx->fmt == F_HTML) fputs(htmlescape(word), out);
	else fputs(word, out);
	if (oneword)
	{
	    oneword = 0;
	    col = 80;
	}
	else col += wordlen;
    }
}

static int writeManSynopsis(FILE *out, Ctx *ctx, const CliDoc *root)
{
    switch (ctx->fmt)
    {
	case F_MAN: fputs("\n.SH \"SYNOPSIS\"\n.PD 0", out); break;
	case F_MDOC: fputs("\n.Sh SYNOPSIS", out); break;
	case F_HTML: fputs("<h2>SYNOPSIS</h2>\n<dl class=\"synopsis\">\n",
			     out); break;
    }
    ctx->nflags = CDRoot_nflags(root);
    ctx->nargs = CDRoot_nargs(root);

    if (ctx->nflags + ctx->nargs == 0)
    {
	if (ctx->fmt == F_HTML) fprintf(out,
		"<dt>%s</dt><dd>&nbsp;</dd>", ctx->name);
	else if (ctx->fmt == F_MDOC) fputs("\n.Nm", out);
	else fprintf(out, "\n.HP 9n\n\\fB%s\\fR", ctx->name);
    }
    else
    {
	int defgroup = CDRoot_defgroup(root);
	for (int i = 0, n = 0; i <= defgroup || n; ++i, n = 0)
	{
	    if (ctx->fmt == F_HTML)
	    {
		fprintf(out, "<dt>%s</dt>\n<dd>\n", ctx->name);
	    }
	    else if (ctx->fmt == F_MDOC)
	    {
		fputs("\n.Nm", out);
	    }
	    else
	    {
		if (i) fputs("\n.br", out);
		fprintf(out, "\n.HP 9n\n\\fB%s\\fR", ctx->name);
	    }
	    char noarg[128] = {0};
	    char rnoarg[128] = {0};
	    int nalen = 0;
	    int rnalen = 0;
	    for (size_t j = 0; j < ctx->nflags; ++j)
	    {
		const CliDoc *flag = CDRoot_flag(root, j);
		int group = CDFlag_group(flag);
		if (group < 0) group = defgroup;
		if (group < 0) group = 0;
		if (group != i) continue;
		if (CDFlag_arg(flag)) continue;
		if (CDFlag_flag(flag) == '-') continue;
		if (CDFlag_optional(flag) == 0)
		{
		    rnoarg[rnalen++] = CDFlag_flag(flag);
		    if (rnalen == sizeof rnoarg) err("too many flags");
		}
		else
		{
		    noarg[nalen++] = CDFlag_flag(flag);
		    if (nalen == sizeof noarg) err("too many flags");
		}
	    }
	    if (rnalen)
	    {
		if (ctx->fmt == F_HTML)
		{
		    fprintf(out, "<span class=\"flag\">-%s</span>\n",
			    rnoarg);
		}
		else fprintf(out, ctx->fmt == F_MDOC ? "\n.Fl %s"
			: "\n\\fB\\-%s\\fR", rnoarg);
		++n;
	    }
	    if (nalen)
	    {
		if (ctx->fmt == F_HTML)
		{
		    fprintf(out, "[<span class=\"flag\">-%s</span>]\n",
			    noarg);
		}
		else fprintf(out, ctx->fmt == F_MDOC ? "\n.Op Fl %s"
			: "\n[\\fB\\-%s\\fR]", noarg);
		++n;
	    }
	    for (size_t j = 0; j < ctx->nflags; ++j)
	    {
		const CliDoc *flag = CDRoot_flag(root, j);
		int group = CDFlag_group(flag);
		if (group < 0) group = defgroup;
		if (group < 0) group = 0;
		if (group != i) continue;
		const char *arg = CDFlag_arg(flag);
		if (!arg)
		{
		    if (CDFlag_flag(flag) == '-')
		    {
			++ctx->separators;
			fputs(ctx->fmt == F_HTML ?
				"[<span class=\"flag\">"
				"--</span>]\n" :
				ctx->fmt == F_MDOC ?
				"\n.Op Fl -" : "\n[\\fB\\-\\-\\fR]", out);
		    }
		    continue;
		}
		int optional = CDFlag_optional(flag);
		if (optional == 0)
		{
		    if (ctx->fmt == F_HTML)
		    {
			fprintf(out, "<span class=\"flag\">-%c</span>"
				"&nbsp;<span class=\"arg\">%s</span>\n",
				CDFlag_flag(flag), arg);
		    }
		    else fprintf(out, ctx->fmt == F_MDOC ? "\n.Fl %c Ar %s"
			    : "\n\\fB\\-%c\\fR\\ \\fI%s\\fR",
			    CDFlag_flag(flag), arg);
		}
		else
		{
		    if (ctx->fmt == F_HTML)
		    {
			fprintf(out, "[<span class=\"flag\">-%c</span>"
				"&nbsp;<span class=\"arg\">%s</span>]\n",
				CDFlag_flag(flag), arg);
		    }
		    else fprintf(out, ctx->fmt == F_MDOC ? "\n.Op Fl %c Ar %s"
			    : "\n[\\fB\\-%c\\fR\\ \\fI%s\\fR]",
			    CDFlag_flag(flag), arg);
		}
		++n;
	    }
	    for (size_t j = 0; j < ctx->nargs; ++j)
	    {
		const CliDoc *arg = CDRoot_arg(root, j);
		int group = CDArg_group(arg);
		if (group < 0) group = defgroup;
		if (group < 0) group = 0;
		if (group != i) continue;
		int optional = CDArg_optional(arg);
		if (optional == 1)
		{
		    if (ctx->fmt == F_HTML)
		    {
			fprintf(out, "[<span class=\"arg\">%s</span>]\n",
				CDArg_arg(arg));
		    }
		    else fprintf(out, ctx->fmt == F_MDOC ? "\n.Op Ar %s"
			    : "\n[\\fI%s\\fR]", CDArg_arg(arg));
		}
		else
		{
		    if (ctx->fmt == F_HTML)
		    {
			fprintf(out, "<span class=\"arg\">%s</span>\n",
				CDArg_arg(arg));
		    }
		    else fprintf(out, ctx->fmt == F_MDOC ? "\n.Ar %s"
			    : "\n\\fI%s\\fR", CDArg_arg(arg));
		}
		++n;
	    }
	    if (ctx->fmt == F_HTML) fputs("</dd>\n", out);
	}
    }
    if (ctx->fmt == F_MAN) fputs("\n.PD", out);
    else if (ctx->fmt == F_HTML) fputs("</dl>\n", out);
    return 0;

error:
    return -1;
}

static int writeManDescription(FILE *out, Ctx *ctx,
	const CliDoc *desc, int idx);

static int writeManList(FILE *out, Ctx *ctx, const CliDoc *list)
{
    size_t len = CDList_length(list);
    for (size_t i = 0; i < len; ++i)
    {
	const CliDoc *entry = CDList_entry(list, i);
	if (writeManDescription(out, ctx, entry, i) < 0) goto error;
    }
    return 0;

error:
    return -1;
}

static void writeManCell(FILE *out, Ctx *ctx, const char *cell)
{
    if (!cell) return;
    while (*cell) switch (*cell)
    {
	case '%':
	    if (!strcmp(cell, "%%name%%"))
	    {
		fprintf(out, ctx->fmt == F_MDOC
			? "\" Ns Nm %s \"" : "\\fB%s\\fR", ctx->name);
		cell += 8;
		continue;
	    }
	    if (!strcmp(cell, "%%arg%%"))
	    {
		fprintf(out, ctx->fmt == F_MDOC
			? "\" Ns Ar %s \"" : "\\fI%s\\fR", ctx->arg);
		cell += 7;
		continue;
	    }
	    ATTR_FALLTHROUGH;
	case '"':
	    if (ctx->fmt == F_MAN) goto writechr;
	    ATTR_FALLTHROUGH;
	case '\\':
	    fputc('\\', out);
	    ATTR_FALLTHROUGH;
	default:
	writechr:
	    fputc(*cell++, out);
    }
}

static void writeManTable(FILE *out, Ctx *ctx, const CliDoc *table)
{
    size_t width = CDTable_width(table);
    size_t height = CDTable_height(table);
    if (ctx->fmt == F_HTML) fputs("<table>\n", out);
    else if (ctx->fmt == F_MDOC)
    {
	struct {
	    size_t len;
	    const char *str;
	} *wspec = 0;
	wspec = xmalloc(width * sizeof *wspec);
	memset(wspec, 0, sizeof *wspec);
	for (size_t y = 0; y < height; ++y)
	{
	    for (size_t x = 0; x < width; ++x)
	    {
		const char *c = CDTable_cell(table, x, y);
		size_t len = strlen(c);
		if (len > wspec[x].len)
		{
		    wspec[x].len = len;
		    wspec[x].str = c;
		}
	    }
	}
	fputs("\n.Bl -column -compact", out);
	for (size_t x = 0; x < width; ++x)
	{
	    fprintf(out, " \"%s\"", wspec[x].str ? wspec[x].str : "");
	}
	free(wspec);
    }
    else
    {
	fputs("\n.TS", out);
	for (size_t x = 0; x < width; ++x)
	{
	    fputc(x ? ' ' : '\n', out);
	    fputc('l', out);
	}
	fputc('.', out);
    }
    for (size_t y = 0; y < height; ++y)
    {
	if (ctx->fmt == F_HTML) fputs("<tr>\n", out);
	for (size_t x = 0; x < width; ++x)
	{
	    if (ctx->fmt == F_HTML) fputs("<td>\n", out);
	    else if (ctx->fmt == F_MDOC) fputs(x ? " Ta \"" : "\n.It \"", out);
	    else fputc(x ? '\t' : '\n', out);
	    if (ctx->fmt == F_HTML) writeManText(out, ctx,
		    CDTable_cell(table, x, y));
	    else writeManCell(out, ctx, CDTable_cell(table, x, y));
	    if (ctx->fmt == F_HTML) fputs("</td>\n", out);
	    else if (ctx->fmt == F_MDOC) fputc('"', out);
	}
	if (ctx->fmt == F_HTML) fputs("</tr>\n", out);
    }
    if (ctx->fmt == F_HTML) fputs("</table>\n", out);
    else if (ctx->fmt == F_MDOC) fputs("\n.El", out);
    else fputs("\n.TE", out);
}

static int writeManDict(FILE *out, Ctx *ctx, const CliDoc *dict)
{
    if (ctx->fmt == F_HTML) fputs("<dl class=\"description\">\n", out);
    else if (ctx->fmt == F_MDOC) fputs("\n.Bl -tag -width Ds -compact", out);
    else fputs("\n.PD 0\n.RS 8n", out);
    size_t len = CDDict_length(dict);
    for (size_t i = 0; i < len; ++i)
    {
	const char *key = CDDict_key(dict, i);
	const CliDoc *val = CDDict_val(dict, i);
	if (ctx->fmt == F_HTML) fprintf(out, "<dt>%s</dt>\n<dd>",
		htmlescape(key));
	else fprintf(out, ctx->fmt == F_MDOC ?
		"\n.It %s" : "\n.TP 8n\n%s", key);
	if (writeManDescription(out, ctx, val, 0) < 0) return -1;
	if (ctx->fmt == F_HTML) fputs("</dd>\n", out);
    }
    if (ctx->fmt == F_HTML) fputs("</dl>\n", out);
    else if (ctx->fmt == F_MDOC) fputs("\n.El", out);
    else fputs("\n.PD\n.RE", out);
    return 0;
}

static int writeManDescription(FILE *out, Ctx *ctx,
	const CliDoc *desc, int idx)
{
    if (!desc) return 0;
    switch (CliDoc_type(desc))
    {
	case CT_TEXT:
	    if (ctx->fmt == F_HTML) fputs("<p>", out);
	    else
	    {
		if (idx) fputs("\n.sp", out);
		fputc('\n', out);
	    }
	    writeManText(out, ctx, CDText_str(desc));
	    if (ctx->fmt == F_HTML) fputs("</p>\n", out);
	    break;
	
	case CT_DICT:
	    if (idx && ctx->fmt != F_HTML) fputs("\n.sp", out);
	    if (writeManDict(out, ctx, desc) < 0) goto error;
	    break;

	case CT_TABLE:
	    writeManTable(out, ctx, desc);
	    break;

	case CT_LIST:
	    if (writeManList(out, ctx, desc) < 0) goto error;
	    break;

	default:
	    break;
    }
    return 0;

error:
    return -1;
}

static int writeManArgDesc(FILE *out, Ctx *ctx, const CliDoc *arg)
{
    if (ctx->fmt == F_HTML) fputs("<dd>\n", out);
    if (writeManDescription(out, ctx,
		CDArg_description(arg), 0) < 0) return -1;
    const CliDoc *min = CDArg_min(arg);
    const CliDoc *max = CDArg_max(arg);
    const CliDoc *def = CDArg_default(arg);
    if (min || max || def)
    {
	if (ctx->fmt == F_HTML) fputs("<dl class=\"meta\">\n", out);
	else if (ctx->fmt == F_MDOC) fputs(
		"\n.sp\n.Bl -tag -width default: -compact", out);
	else fputs("\n.sp\n.PD 0\n.RS 8n", out);
	if (min)
	{
	    if (ctx->fmt == F_HTML) fputs("<dt>min:</dt><dd>", out);
	    else if (ctx->fmt == F_MDOC) fputs("\n.It min:", out);
	    else fputs("\n.TP 10n\nmin:", out);
	    if (writeManDescription(out, ctx, min, 0) < 0) return -1;
	    if (ctx->fmt == F_HTML) fputs("</dd>\n", out);
	}
	if (max)
	{
	    if (ctx->fmt == F_HTML) fputs("<dt>max:</dt><dd>", out);
	    else if (ctx->fmt == F_MDOC) fputs("\n.It max:", out);
	    else fputs("\n.TP 10n\nmax:", out);
	    if (writeManDescription(out, ctx, max, 0) < 0) return -1;
	    if (ctx->fmt == F_HTML) fputs("</dd>\n", out);
	}
	if (def)
	{
	    if (ctx->fmt == F_HTML) fputs("<dt>default:</dt><dd>", out);
	    else if (ctx->fmt == F_MDOC) fputs("\n.It default:", out);
	    else fputs("\n.TP 10n\ndefault:", out);
	    if (writeManDescription(out, ctx, def, 0) < 0) return -1;
	    if (ctx->fmt == F_HTML) fputs("</dd>\n", out);
	}
	if (ctx->fmt == F_HTML) fputs("</dl>\n", out);
	else if (ctx->fmt == F_MDOC) fputs("\n.El", out);
	else fputs("\n.PD\n.RE", out);
    }
    if (ctx->fmt == F_HTML) fputs("</dd>\n", out);
    return 0;
}

static int write(FILE *out, const CliDoc *root, Fmt fmt, const FmtOpts *opts)
{
    assert(CliDoc_type(root) == CT_ROOT);
    
    Ctx ctx = Ctx_init(root, fmt, opts);

    const CliDoc *date = CDRoot_date(root);
    if (!date || CliDoc_type(date) != CT_DATE) err("missing date");
    const CliDoc *name = CDRoot_name(root);
    if (!istext(name)) err("missing name");
    ctx.name = CDText_str(name);
    const CliDoc *version = CDRoot_version(root);
    const CliDoc *comment = CDRoot_comment(root);
    if (!istext(comment)) err("missing comment");

    time_t dv = CDDate_date(date);
    struct tm *tm = localtime(&dv);
    if (fmt == F_HTML)
    {
	const char *title = strToUpper(htmlescape(ctx.name));
	if (opts->style) fprintf(out, HTML_HEADER_STYLE(opts->style, title));
	else if (opts->styleuri) fprintf(out,
		HTML_HEADER_STYLEURI(htmlescape(opts->styleuri), title));
	else fprintf(out, HTML_HEADER_STYLE(HTML_DEFAULT_STYLE, title));
	fprintf(out, "<h2>NAME</h2>\n<dl class=\"name\">\n"
		"<dt><span class=\"name\">%s</span> &ndash;</dt>\n"
		"<dd>", htmlescape(ctx.name));
	writeManText(out, &ctx, CDText_str(comment));
	fputs("</dd>\n</dl>\n", out);
    }
    else if (fmt == F_MDOC)
    {
	size_t mlen = strftime(strbuf, sizeof strbuf, "%B", tm);
	snprintf(strbuf + mlen, sizeof strbuf - mlen,
		" %d, %d", tm->tm_mday, tm->tm_year + 1900);
	fprintf(out, ".Dd %s", strbuf);
	fprintf(out, "\n.Dt %s 1\n.Os", strToUpper(ctx.name));
	if (opts->os)
	{
	    fprintf(out, " %s", ctx.name);
	    if (istext(version)) fprintf(out, " %s", CDText_str(version));
	}
	fprintf(out, "\n.Sh NAME\n.Nm %s\n.Nd %s",
		ctx.name, CDText_str(comment));
    }
    else
    {
	fprintf(out, ".TH \"%s\" \"1\" ", strToUpper(ctx.name));
	size_t mlen = strftime(strbuf, sizeof strbuf, "%B", tm);
	snprintf(strbuf + mlen, sizeof strbuf - mlen,
		" %d, %d", tm->tm_mday, tm->tm_year + 1900);
	fprintf(out, "\"%s\" \"%s", strbuf, ctx.name);
	if (istext(version)) fprintf(out, " %s", CDText_str(version));
	fputs("\"\n.nh\n.if n .ad l\n.SH \"NAME\"", out);
	fprintf(out, "\n\\fB%s\\fR\n\\- %s", ctx.name, CDText_str(comment));
    }

    if (writeManSynopsis(out, &ctx, root) < 0) goto error;

    if (fmt == F_HTML) fputs("<h2>DESCRIPTION</h2>\n", out);
    else if (fmt == F_MDOC) fputs("\n.Sh DESCRIPTION", out);
    else fputs("\n.SH \"DESCRIPTION\"", out);
    if (writeManDescription(out, &ctx,
		CDRoot_description(root), 0) < 0) goto error;

    if (ctx.nflags + ctx.nargs - ctx.separators > 0)
    {
	fputs(fmt == F_HTML ? "<p>The options are as follows:</p>\n"
		: "\n.sp\nThe options are as follows:", out);
	if (fmt == F_HTML) fputs("<dl class=\"description\">\n", out);
	if (fmt == F_MDOC) fputs("\n.Bl -tag -width Ds", out);
	for (size_t i = 0; i < ctx.nflags; ++i)
	{
	    const CliDoc *flag = CDRoot_flag(root, i);
	    if (CDFlag_flag(flag) == '-') continue;
	    if (fmt == F_MAN) fputs("\n.TP 8n", out);
	    const char *arg = CDFlag_arg(flag);
	    if (arg)
	    {
		ctx.arg = arg;
		if (fmt == F_HTML)
		{
		    fprintf(out, "<dt><span class=\"flag\">-%c</span>"
			    "&nbsp;<span class=\"arg\">%s</span></dt>\n",
			    CDFlag_flag(flag), htmlescape(arg));
		}
		else fprintf(out, fmt == F_MDOC ? "\n.It Fl %c Ar %s"
			: "\n\\fB\\-%c\\fR \\fI%s\\fR\\ ",
			CDFlag_flag(flag), arg);
	    }
	    else
	    {
		ctx.arg = 0;
		if (fmt == F_HTML)
		{
		    fprintf(out,
			    "<dt><span class=\"flag\">-%c</span></dt>\n",
			    CDFlag_flag(flag));
		}
		else fprintf(out, fmt == F_MDOC ? "\n.It Fl %c"
			: "\n\\fB\\-%c\\fR\\ ", CDFlag_flag(flag));
	    }
	    if (writeManArgDesc(out, &ctx, flag) < 0) goto error;
	}
	for (size_t i = 0; i < ctx.nargs; ++i)
	{
	    if (fmt == F_MAN) fputs("\n.TP 8n", out);
	    const CliDoc *arg = CDRoot_arg(root, i);
	    ctx.arg = CDArg_arg(arg);
	    if (fmt == F_HTML)
	    {
		fprintf(out, "<dt><span class=\"arg\">%s</span></dt>\n",
			htmlescape(ctx.arg));
	    }
	    else fprintf(out, fmt == F_MDOC ?
		    "\n.It Ar %s" : "\n\\fI%s\\fR\\ ", ctx.arg);
	    if (writeManArgDesc(out, &ctx, arg) < 0) goto error;
	}
	if (fmt == F_HTML) fputs("</dl>\n", out);
	if (fmt == F_MDOC) fputs("\n.El", out);
    }

    const CliDoc *license = CDRoot_license(root);
    const CliDoc *www = CDRoot_www(root);
    if (istext(version) || istext(license) || istext(www))
    {
	if (fmt == F_HTML)
	{
	    fputs("<h3>Additional information</h3>\n<dl class=\"meta\">\n",
		    out);
	}
	else if (fmt == F_MDOC) fputs("\n.Ss Additional information\n"
		".Bl -tag -width Version: -compact", out);
	else fputs("\n.SS \"Additional information\"\n.PD 0", out);
	if (istext(version))
	{
	    if (fmt == F_HTML) fprintf(out, "<dt>Version:</dt>\n"
		    "<dd><span class=\"name\">%s</span> %s</dd>\n",
		    ctx.name, htmlescape(CDText_str(version)));
	    else if (fmt == F_MDOC) fprintf(out, "\n.It Version:\n.Nm\n%s",
		    CDText_str(version));
	    else fprintf(out, "\n.TP 10n\nVersion:\n\\fB%s\\fR\n%s",
		    ctx.name, CDText_str(version));
	}
	if (istext(license))
	{
	    if (fmt == F_HTML) fputs("<dt>License:</dt><dd>", out);
	    else if (fmt == F_MDOC) fputs("\n.It License:\n", out);
	    else fputs("\n.TP 10n\nLicense:\n", out);
	    writeManText(out, &ctx, CDText_str(license));
	    if (fmt == F_HTML) fputs("</dd>\n", out);
	}
	if (istext(www))
	{
	    if (fmt == F_HTML)
	    {
		char *escaped = htmlescape(CDText_str(www));
		fprintf(out, "<dt>WWW:</dt><dd><a href=\"%s\">%s</a></dd>\n",
			escaped, escaped);
	    }
	    else
	    {
		if (fmt == F_MDOC) fputs("\n.It WWW:\n.Lk ", out);
		else fputs("\n.TP 10n\nWWW:\n\\fB", out);
		writeManText(out, &ctx, CDText_str(www));
		if (fmt == F_MAN) fputs("\\fR", out);
	    }
	}
	if (fmt == F_HTML) fputs("</dl>\n", out);
	else if (fmt == F_MDOC) fputs("\n.El", out);
	else fputs("\n.PD", out);
    }

    size_t nvars = CDRoot_nvars(root);
    if (nvars)
    {
	struct {
	    const char *tag;
	    size_t tagwidth;
	} wspec = {0, 0};

	if (fmt != F_HTML)
	{
	    for (size_t i = 0; i < nvars; ++i)
	    {
		const CliDoc *var = CDRoot_var(root, i);
		const char *vname = CDNamed_name(var);
		size_t namelen = strlen(vname);
		if (namelen > wspec.tagwidth)
		{
		    wspec.tag = vname;
		    wspec.tagwidth = namelen;
		}
	    }
	    wspec.tagwidth += 2;
	}

	if (fmt == F_HTML)
	{
	    fputs("<h2>ENVIRONMENT</h2>\n<dl class=\"environment\">\n", out);
	}
	else if (fmt == F_MDOC)
	{
	    fprintf(out, "\n.Sh ENVIRONMENT\n.Bl -tag -width \"%s\"",
		    wspec.tag);
	}
	else fputs("\n.SH \"ENVIRONMENT\"", out);
	for (size_t i = 0; i < nvars; ++i)
	{
	    if (fmt == F_MAN) fprintf(out, "\n.TP %un",
		    (unsigned)wspec.tagwidth);
	    const CliDoc *var = CDRoot_var(root, i);
	    if (fmt == F_HTML)
	    {
		fprintf(out,
			"<dt><span class=\"name\">%s</span></dt>\n<dd>\n",
			htmlescape(CDNamed_name(var)));
	    }
	    else fprintf(out, fmt == F_MDOC ? "\n.It Ev %s" : "\n\\fB%s\\fR",
		    CDNamed_name(var));
	    if (writeManDescription(out, &ctx,
			CDNamed_description(var), 0) < 0) goto error;
	    if (fmt == F_HTML) fputs("</dd>\n", out);
	}
	if (fmt == F_HTML) fputs("</dl>\n", out);
	if (fmt == F_MDOC) fputs("\n.El", out);
    }

    size_t nfiles = CDRoot_nfiles(root);
    if (nfiles)
    {
	if (fmt == F_HTML)
	{
	    fputs("<h2>FILES</h2>\n<dl class=\"description\">\n", out);
	}
	else if (fmt == F_MDOC)
	{
	    fputs("\n.Sh FILES\n.Bl -tag -width Ds", out);
	}
	else fputs("\n.SH \"FILES\"", out);
	for (size_t i = 0; i < nfiles; ++i)
	{
	    if (fmt == F_MAN) fputs("\n.TP 8n", out);
	    const CliDoc *file = CDRoot_file(root, i);
	    if (fmt == F_HTML)
	    {
		fprintf(out,
			"<dt><span class=\"file\">%s</span></dt>\n<dd>\n",
			htmlescape(CDNamed_name(file)));
	    }
	    else fprintf(out, fmt == F_MDOC ? "\n.It Pa %s" : "\n\\fI%s\\fR",
		    CDNamed_name(file));
	    if (writeManDescription(out, &ctx,
			CDNamed_description(file), 0) < 0) goto error;
	    if (fmt == F_HTML) fputs("</dd>\n", out);
	}
	if (fmt == F_HTML) fputs("</dl>\n", out);
	if (fmt == F_MDOC) fputs("\n.El", out);
    }

    size_t nrefs = CDRoot_nrefs(root);
    if (nrefs)
    {
	if (fmt == F_HTML) fputs("<h2>SEE ALSO</h2>\n<p>", out);
	else if (fmt == F_MDOC) fputs("\n.Sh SEE ALSO", out);
	else fputs("\n.SH \"SEE ALSO\"", out);
	for (size_t i = 0; i < nrefs; ++i)
	{
	    const CliDoc *ref = CDRoot_ref(root, i);
	    if (fmt == F_HTML)
	    {
		if (i) fputs(", ", out);
		fprintf(out, "<span class=\"name\">%s</span>",
		    htmlescape(CDMRef_name(ref)));
		fprintf(out, "(%s)", htmlescape(CDMRef_section(ref)));
	    }
	    else fprintf(out, fmt == F_MDOC
		    ? (i ? " ,\n.Xr %s %s" : "\n.Xr %s %s")
		    : (i ? "\\fR,\n\\fB%s\\fP(%s)" : "\n\\fB%s\\fP(%s)"),
		    CDMRef_name(ref), CDMRef_section(ref));
	}
	if (fmt == F_HTML) fputs("</p>\n", out);
    }

    const CliDoc *author = CDRoot_author(root);
    if (istext(author))
    {
	if (fmt == F_HTML) fputs("<h2>AUTHORS</h2>\n", out);
	else if (fmt == F_MDOC) fputs("\n.Sh AUTHORS\n.An ", out);
	else fputs("\n.SH \"AUTHORS\"\n", out);
	const char *astr = CDText_str(author);
	const char *es = strchr(astr, '<');
	const char *ea = strchr(astr, '@');
	const char *ee = strchr(astr, '>');
	if (es && ea && ee && es < ea && ea < ee)
	{
	    if (fmt == F_HTML)
	    {
		fputs(htmlnescape(astr, es-astr), out);
		char *email = htmlnescape(es+1, ee-es-1);
		fprintf(out, " &lt;<a href=\"mailto:%s\">%s</a>&gt;\n",
			email, email);
	    }
	    else
	    {
		fwrite(astr, 1, es-astr, out);
		if (fmt == F_MDOC) fputs(" Aq Mt ", out);
		else fputs("<\\fI", out);
		fwrite(es+1, 1, ee-es-1, out);
		if (fmt == F_MAN) fputs("\\fR>", out);
	    }
	}
	else fputs(fmt == F_HTML ? htmlescape(astr) : astr, out);
    }

    if (fmt == F_HTML)
    {
	fprintf(out, "<dl class=\"footer\">\n<dt>Origin:</dt>\n<dd>%s",
		htmlescape(ctx.name));
	if (istext(version))
	{
	    fprintf(out, " %s", htmlescape(CDText_str(version)));
	}
	size_t mlen = strftime(strbuf, sizeof strbuf, "%B", tm);
	snprintf(strbuf + mlen, sizeof strbuf - mlen,
		" %d, %d", tm->tm_mday, tm->tm_year + 1900);
	fprintf(out, "</dd>\n<dt>Date:</dt>\n<dd>%s</dd>\n", strbuf);
	fprintf(out, "<dt>Title:</dt>\n<dd>%s(1)</dd>\n",
		strToUpper(ctx.name));
	fputs("</dl>\n</body>\n</html>", out);
    }

    fputc('\n', out);
    return 0;

error:
    return -1;
}

int writeMan(FILE *out, const CliDoc *root, const char *args)
{
    if (args)
    {
	fputs("The man format does not support any arguments.\n", stderr);
	return -1;
    }
    return write(out, root, F_MAN, 0);
}

int writeMdoc(FILE *out, const CliDoc *root, const char *args)
{
    FmtOpts opts;
    memset(&opts, 0, sizeof opts);

    if (args)
    {
	if (!strcmp(args, "os")) opts.os = 1;
	else
	{
	    fprintf(stderr, "Unknown arguments for mdoc format: %s\n", args);
	    fputs("Supported: os  [Override operating system with "
		    "tool name and version]\n", stderr);
	    return -1;
	}
    }
    return write(out, root, F_MDOC, &opts);
}

static size_t parseOpt(char *buf, const char *argp, char **valp, char **nextp)
{
    size_t n;
    for (n = 0, *valp = *nextp = 0; argp[n] && !*nextp; ++n)
    {
	switch (argp[n])
	{
	    case '\\':
		if (!argp[n+1]) *buf++ = '\\';
		else *buf++ = argp[++n];
		break;
	    case '=':
		if (!*valp)
		{
		    *buf++ = 0;
		    *valp = buf;
		}
		else *buf++ = '=';
		break;
	    case ':':
		*buf++ = 0;
		*nextp = buf;
		break;
	    default:
		*buf++ = argp[n];
	}
    }
    *buf = 0;
    return n;
}

int writeHtml(FILE *out, const CliDoc *root, const char *args)
{
    FmtOpts opts;
    memset(&opts, 0, sizeof opts);
    char *optstr = 0;
    char *style = 0;
    FILE *css = 0;
    int rc = -1;
    char *valp;
    char *nextp;

    if (args)
    {
	size_t arglen = strlen(args);
	optstr = xmalloc(arglen + 1);
	const char *argp = args;
	char *buf = optstr;
	size_t len;
	while (buf && (len = parseOpt(buf, argp, &valp, &nextp)))
	{
	    if (!valp) goto error;
	    if (!strcmp(buf, "style"))
	    {
		css = fopen(valp, "r");
		if (!css) goto styleerr;
		size_t stylesz = 0;
		for (;;)
		{
		    style = xrealloc(style, stylesz + 1024);
		    size_t chunk = fread(style + stylesz, 1, 1024, css);
		    stylesz += chunk;
		    if (chunk < 1024) break;
		}
		if (ferror(css)) goto styleerr;
		fclose(css);
		css = 0;
		style[stylesz] = 0;
		opts.style = style;
	    }
	    else if (!strcmp(buf, "styleuri")) opts.styleuri = valp;
	    else goto error;
	    buf = nextp;
	    argp += len;
	}
    }

    rc = write(out, root, F_HTML, &opts);
    goto done;

styleerr:
    fprintf(stderr, "Error reading %s\n", valp);
    goto done;

error:
    fprintf(stderr, "Invalid arguments for html: %s\n", args);
    fputs("Supported:  style=file, styleuri=uri\n",
	    stderr);
done:
    if (css) fclose(css);
    free(style);
    free(optstr);
    return rc;
}

