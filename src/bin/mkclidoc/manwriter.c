#include "manwriter.h"

#include "clidoc.h"
#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct Ctx
{
    const CliDoc *root;
    const char *name;
    const char *arg;
    size_t nflags;
    size_t nargs;
    int mdoc;
} Ctx;
#define Ctx_init(root, mdoc) {(root), 0, 0, 0, 0, (mdoc)}

#define err(m) do { \
    fprintf(stderr, "Cannot write man: %s\n", (m)); goto error; } while (0)
#define istext(m) ((m) && CliDoc_type(m) == CT_TEXT)
#define ismpunct(c) (ispunct(c) && (c) != '\\' && (c) != '%' && (c) != '`')
#define istpunct(s) (ismpunct(*(s)) && \
	(!(s)[1] || (s)[1] == ' ' || (s)[1] == '\t'))

static char strbuf[8192];

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

static char *fetchManTextWord(const char **s)
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
	if (**s == '\\') strbuf[wordlen++] = '\\';
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

	if (*str == '.' && (!str[1] || str[1] == ' ' || str[1] == '\t'))
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
	    if (col && space) fputc(' ', out), ++col;
	    size_t punctlen = 1;
	    while (ismpunct(str[punctlen]) && str[punctlen] != '.') ++punctlen;
	    if (col && col + punctlen > 78)
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

	char *word = fetchManTextWord(&str);
	if (!word) break;
	int writename = !strcmp(word, "%%name%%");
	int writearg = !strcmp(word, "%%arg%%") && ctx->arg;
	if (writename || writearg)
	{
	    if (col) fputc('\n', out);
	    if (writename)
	    {
		if (ctx->mdoc) fputs(".Nm", out);
		else fprintf(out, "\\fB%s\\fR", ctx->name);
	    }
	    else if (writearg)
	    {
		fprintf(out, ctx->mdoc ? ".Ar %s" : "\\fI%s\\fR", ctx->arg);
	    }
	    if (ctx->mdoc)
	    {
		if (ismpunct(*str))
		{
		    fputc(' ', out);
		    oneword = 1;
		}
		else if (*str == ' ' || *str == '\t') nl = 1;
		else
		{
		    fputs(" Ns ", out);
		    oneword = 1;
		}
	    }
	    else
	    {
		if (*str == ' ' || *str == '\t') nl = 1;
		else oneword = 1;
	    }
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
		fprintf(out, ctx->mdoc ? ".Xr %s %s" : "\\fB%s\\fP(%s)\\fR",
			CDMRef_name(ref), CDMRef_section(ref));
		if (ctx->mdoc)
		{
		    if (ismpunct(*str))
		    {
			fputc(' ', out);
			oneword = 1;
		    }
		    else if (*str == ' ' || *str == '\t') nl = 1;
		    else
		    {
			fputs(" Ns ", out);
			oneword = 1;
		    }
		}
		else
		{
		    if (*str == ' ' || *str == '\t') nl = 1;
		    else oneword = 1;
		}
		continue;
	    }
	}
	if (nl || col + !!col + wordlen > 78)
	{
	    fputc('\n', out);
	    col = 0;
	    nl = 0;
	}
	if (col && space) ++col, fputc(' ', out);
	fputs(word, out);
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
    if (ctx->mdoc) fputs("\n.Sh SYNOPSIS", out);
    else fputs("\n.SH \"SYNOPSIS\"\n.PD 0", out);

    ctx->nflags = CDRoot_nflags(root);
    ctx->nargs = CDRoot_nargs(root);

    if (ctx->nflags + ctx->nargs == 0)
    {
	if (ctx->mdoc) fputs("\n.Nm", out);
	else fprintf(out, "\n.HP 9n\n\\fB%s\\fR", ctx->name);
    }
    else
    {
	int defgroup = CDRoot_defgroup(root);
	for (int i = 0, n = 0; i <= defgroup || n; ++i, n = 0)
	{
	    if (ctx->mdoc) fputs("\n.Nm", out);
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
		fprintf(out, ctx->mdoc ? "\n.Fl %s"
			: "\n\\fB\\-%s\\fR", rnoarg);
		++n;
	    }
	    if (nalen)
	    {
		fprintf(out, ctx->mdoc ? "\n.Op Fl %s"
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
		if (!arg) continue;
		int optional = CDFlag_optional(flag);
		if (optional == 0)
		{
		    fprintf(out, ctx->mdoc ? "\n.Fl %c Ar %s"
			    : "\n\\fB\\-%c\\fR\\ \\fI%s\\fR",
			    CDFlag_flag(flag), arg);
		}
		else
		{
		    fprintf(out, ctx->mdoc ? "\n.Op Fl %c Ar %s"
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
		    fprintf(out, ctx->mdoc ? "\n.Op Ar %s"
			    : "\n[\\fI%s\\fR]", CDArg_arg(arg));
		}
		else
		{
		    fprintf(out, ctx->mdoc ? "\n.Ar %s"
			    : "\n\\fI%s\\fR", CDArg_arg(arg));
		}
		++n;
	    }
	}
    }
    if (!ctx->mdoc) fputs("\n.PD", out);
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
		fprintf(out, ctx->mdoc ? "\" Ns Nm %s \"" : "\\fB%s\\fR",
			ctx->name);
		cell += 8;
		continue;
	    }
	    if (!strcmp(cell, "%%arg%%"))
	    {
		fprintf(out, ctx->mdoc ? "\" Ns Ar %s \"" : "\\fI%s\\fR",
			ctx->arg);
		cell += 7;
		continue;
	    }
	    ATTR_FALLTHROUGH;
	case '"':
	    if (!ctx->mdoc) goto writechr;
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
    if (ctx->mdoc)
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
	for (size_t x = 0; x < width; ++x)
	{
	    if (ctx->mdoc) fputs(x ? " Ta \"" : "\n.It \"", out);
	    else fputc(x ? '\t' : '\n', out);
	    writeManCell(out, ctx, CDTable_cell(table, x, y));
	    if (ctx->mdoc) fputc('"', out);
	}
    }
    if (ctx->mdoc) fputs("\n.El", out);
    else fputs("\n.TE", out);
}

static int writeManDict(FILE *out, Ctx *ctx, const CliDoc *dict)
{
    if (ctx->mdoc) fputs("\n.Bl -tag -width Ds -compact", out);
    else fputs("\n.PD 0\n.RS 8n", out);
    size_t len = CDDict_length(dict);
    for (size_t i = 0; i < len; ++i)
    {
	const char *key = CDDict_key(dict, i);
	const CliDoc *val = CDDict_val(dict, i);
	fprintf(out, ctx->mdoc ? "\n.It %s" : "\n.TP 8n\n%s", key);
	if (writeManDescription(out, ctx, val, 0) < 0) return -1;
    }
    if (ctx->mdoc) fputs("\n.El", out);
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
	    if (idx) fputs("\n.sp", out);
	    fputc('\n', out);
	    writeManText(out, ctx, CDText_str(desc));
	    break;
	
	case CT_DICT:
	    if (idx) fputs("\n.sp", out);
	    if (writeManDict(out, ctx, desc) < 0) goto error;
	    break;

	case CT_TABLE:
	    if (idx) fputs("\n.sp", out);
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
    if (writeManDescription(out, ctx,
		CDArg_description(arg), 0) < 0) return -1;
    const CliDoc *min = CDArg_min(arg);
    const CliDoc *max = CDArg_max(arg);
    const CliDoc *def = CDArg_default(arg);
    if (min || max || def)
    {
	if (ctx->mdoc) fputs("\n.sp\n.Bl -tag -width default: -compact", out);
	else fputs("\n.sp\n.PD 0\n.RS 8n", out);
	if (min)
	{
	    if (ctx->mdoc) fputs("\n.It min:", out);
	    else fputs("\n.TP 10n\nmin:", out);
	    if (writeManDescription(out, ctx, min, 0) < 0) return -1;
	}
	if (max)
	{
	    if (ctx->mdoc) fputs("\n.It max:", out);
	    else fputs("\n.TP 10n\nmax:", out);
	    if (writeManDescription(out, ctx, max, 0) < 0) return -1;
	}
	if (def)
	{
	    if (ctx->mdoc) fputs("\n.It default:", out);
	    else fputs("\n.TP 10n\ndefault:", out);
	    if (writeManDescription(out, ctx, def, 0) < 0) return -1;
	}
	if (ctx->mdoc) fputs("\n.El", out);
	else fputs("\n.PD\n.RE", out);
    }
    return 0;
}

static int write(FILE *out, const CliDoc *root, int mdoc)
{
    assert(CliDoc_type(root) == CT_ROOT);
    
    Ctx ctx = Ctx_init(root, mdoc);

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
    if (mdoc)
    {
	size_t mlen = strftime(strbuf, sizeof strbuf, "%B", tm);
	snprintf(strbuf + mlen, sizeof strbuf - mlen,
		" %d, %d", tm->tm_mday, tm->tm_year + 1900);
	fprintf(out, ".Dd %s", strbuf);
	fprintf(out, "\n.Dt %s 1\n.Os", strToUpper(ctx.name));
	if (mdoc == 2)
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

    if (mdoc) fputs("\n.Sh DESCRIPTION", out);
    else fputs("\n.SH \"DESCRIPTION\"", out);
    if (writeManDescription(out, &ctx,
		CDRoot_description(root), 0) < 0) goto error;

    if (ctx.nflags + ctx.nargs > 0)
    {
	fputs("\n.sp\nThe options are as follows:", out);
	if (mdoc) fputs("\n.Bl -tag -width Ds", out);
	for (size_t i = 0; i < ctx.nflags; ++i)
	{
	    if (!mdoc) fputs("\n.TP 8n", out);
	    const CliDoc *flag = CDRoot_flag(root, i);
	    const char *arg = CDFlag_arg(flag);
	    if (arg)
	    {
		ctx.arg = arg;
		fprintf(out, mdoc ? "\n.It Fl %c Ar %s"
			: "\n\\fB\\-%c\\fR \\fI%s\\fR\\ ",
			CDFlag_flag(flag), arg);
	    }
	    else
	    {
		ctx.arg = 0;
		fprintf(out, mdoc ? "\n.It Fl %c" : "\n\\fB\\-%c\\fR\\ ",
			CDFlag_flag(flag));
	    }
	    if (writeManArgDesc(out, &ctx, flag) < 0) goto error;
	}
	for (size_t i = 0; i < ctx.nargs; ++i)
	{
	    if (!mdoc) fputs("\n.TP 8n", out);
	    const CliDoc *arg = CDRoot_arg(root, i);
	    ctx.arg = CDArg_arg(arg);
	    fprintf(out, mdoc ? "\n.It Ar %s" : "\n\\fI%s\\fR\\ ", ctx.arg);
	    if (writeManArgDesc(out, &ctx, arg) < 0) goto error;
	}
	if (mdoc) fputs("\n.El", out);
    }

    const CliDoc *license = CDRoot_license(root);
    const CliDoc *www = CDRoot_www(root);
    if (istext(version) || istext(license) || istext(www))
    {
	if (mdoc) fputs("\n.Ss Additional information\n"
		".Bl -tag -width Version: -compact", out);
	else fputs("\n.SS \"Additional information\"\n.PD 0", out);
	if (istext(version))
	{
	    if (mdoc) fprintf(out, "\n.It Version:\n.Nm\n%s",
		    CDText_str(version));
	    else fprintf(out, "\n.TP 10n\nVersion:\n\\fB%s\\fR\n%s",
		    ctx.name, CDText_str(version));
	}
	if (istext(license))
	{
	    if (mdoc) fputs("\n.It License:\n", out);
	    else fputs("\n.TP 10n\nLicense:\n", out);
	    writeManText(out, &ctx, CDText_str(license));
	}
	if (istext(www))
	{
	    if (mdoc) fputs("\n.It WWW:\n.Lk ", out);
	    else fputs("\n.TP 10n\nWWW:\n\\fB", out);
	    writeManText(out, &ctx, CDText_str(www));
	    if (!mdoc) fputs("\\fR", out);
	}
	if (mdoc) fputs("\n.El", out);
	else fputs("\n.PD", out);
    }

    size_t nfiles = CDRoot_nfiles(root);
    if (nfiles)
    {
	if (mdoc) fputs("\n.Sh FILES", out);
	else fputs("\n.SH \"FILES\"", out);
	if (mdoc) fputs("\n.Bl -tag -width Ds", out);
	for (size_t i = 0; i < nfiles; ++i)
	{
	    if (!mdoc) fputs("\n.TP 8n", out);
	    const CliDoc *file = CDRoot_file(root, i);
	    fprintf(out, mdoc ? "\n.It Pa %s" : "\n\\fI%s\\fR",
		    CDNamed_name(file));
	    if (writeManDescription(out, &ctx,
			CDNamed_description(file), 0) < 0) goto error;
	}
	if (mdoc) fputs("\n.El", out);
    }

    size_t nrefs = CDRoot_nrefs(root);
    if (nrefs)
    {
	if (mdoc) fputs("\n.Sh SEE ALSO", out);
	else fputs("\n.SH \"SEE ALSO\"", out);
	for (size_t i = 0; i < nrefs; ++i)
	{
	    const CliDoc *ref = CDRoot_ref(root, i);
	    fprintf(out, mdoc ? (i ? " ,\n.Xr %s %s" : "\n.Xr %s %s")
		    : (i ? "\\fR,\n\\fB%s\\fP(%s)" : "\n\\fB%s\\fP(%s)"),
		    CDMRef_name(ref), CDMRef_section(ref));
	}
    }

    const CliDoc *author = CDRoot_author(root);
    if (istext(author))
    {
	if (mdoc) fputs("\n.Sh AUTHORS\n.An ", out);
	else fputs("\n.SH \"AUTHORS\"\n", out);
	const char *astr = CDText_str(author);
	const char *es = strchr(astr, '<');
	const char *ea = strchr(astr, '@');
	const char *ee = strchr(astr, '>');
	if (es && ea && ee && es < ea && ea < ee)
	{
	    fwrite(astr, 1, es-astr, out);
	    if (mdoc) fputs(" Aq Mt ", out);
	    else fputs("<\\fI", out);
	    fwrite(es+1, 1, ee-es-1, out);
	    if (!mdoc) fputs("\\fR>", out);
	}
	else fputs(astr, out);
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
    return write(out, root, 0);
}

int writeMdoc(FILE *out, const CliDoc *root, const char *args)
{
    int mdoc = 1;
    if (args)
    {
	if (!strcmp(args, "os")) ++mdoc;
	else
	{
	    fprintf(stderr, "Unknown arguments for mdoc format: %s\n", args);
	    fputs("Supported: os  [Override operating system with "
		    "tool name and version]\n", stderr);
	    return -1;
	}
    }
    return write(out, root, mdoc);
}

