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
    const char *name;
    const char *arg;
    size_t nflags;
    size_t nargs;
    int mdoc;
} Ctx;
#define Ctx_init(mdoc) {0, 0, 0, 0, (mdoc)}

#define err(m) do { \
    fprintf(stderr, "Cannot write man: %s\n", (m)); goto error; } while (0)
#define istext(m) ((m) && CliDoc_type(m) == CT_TEXT)

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

static void mputc(FILE *out, int c)
{
    fputc(c, out);
    if (c == '\\') fputc(c, out);
}

static void mputs(FILE *out, const char *s)
{
    while (*s) mputc(out, *s++);
}

static void mnputs(FILE *out, const char *s, size_t n)
{
    while (n--) mputc(out, *s++);
}

static void writeManText(FILE *out, Ctx *ctx, const char *str)
{
    const char *nmpos = 0;
    const char *argpos = 0;
    const char *period = 0;
    int nl = 1;
    for (;;)
    {
	if (nmpos < str) nmpos = strstr(str, "%%name%%");
	if (argpos < str) argpos = strstr(str, "%%arg%%");
	if (period < str) period = strstr(str, ". ");
	const char *endp = nmpos;
	if (!endp || (argpos && argpos < endp)) endp = argpos;
	if (!endp || (period && period < endp)) endp = period;
	if (!endp)
	{
	    mputs(out, str);
	    break;
	}
	if (endp != str)
	{
	    mnputs(out, str, endp-str);
	    nl = 0;
	}
	str = endp;
	if (endp == period)
	{
	    fputc(*str++, out);
	    fputc('\n', out);
	    nl = 1;
	    if (!*++str) break;
	}
	else if (endp == nmpos)
	{
	    if (!nl) fputc('\n', out);
	    if (ctx->mdoc) fputs(".Nm", out);
	    else fprintf(out, "\\fB%s\\fR", ctx->name);
	    str += 8;
	    if (*str)
	    {
		if (*str != ' ')
		{
		    if (ctx->mdoc)
		    {
			if (*str != '.' && *str != ','
				&& *str != ':' && *str != ';')
			{
			    fputs(" Ns No ", out);
			}
			else fputc(' ', out);
		    }
		    while (*str && *str != ' ')
		    {
			mputc(out, *str++);
		    }
		    if (*str) ++str;
		}
		else ++str;
	    }
	    if (!*str) break;
	    fputc('\n', out);
	    nl = 1;
	}
	else if (endp == argpos)
	{
	    if (!nl) fputc('\n', out);
	    fprintf(out, ctx->mdoc ? ".Ar %s" : "\\fI%s\\fR", ctx->arg);
	    str += 7;
	    if (*str)
	    {
		if (*str != ' ')
		{
		    if (ctx->mdoc)
		    {
			if (*str != '.' && *str != ','
				&& *str != ':' && *str != ';')
			{
			    fputs(" Ns No ", out);
			}
			else fputc(' ', out);
		    }
		    while (*str && *str != ' ')
		    {
			mputc(out, *str++);
		    }
		    if (*str) ++str;
		}
		else ++str;
	    }
	    if (!*str) break;
	    fputc('\n', out);
	    nl = 1;
	}
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
    else fputs("\n.PD\n.PP\n.RE", out);
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
	else fputs("\n.PD\n.PP\n.RE", out);
    }
    return 0;
}

static int write(FILE *out, const CliDoc *root, int mdoc)
{
    assert(CliDoc_type(root) == CT_ROOT);
    
    Ctx ctx = Ctx_init(mdoc);

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
	fprintf(out, "\n.Dt %s 1\n.Os %s", strToUpper(ctx.name), ctx.name);
	if (istext(version)) fprintf(out, " %s", CDText_str(version));
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
			: "\n\\fB\\-%c\\fR \\fI%s\\fR",
			CDFlag_flag(flag), arg);
	    }
	    else
	    {
		ctx.arg = 0;
		fprintf(out, mdoc ? "\n.It Fl %c" : "\n\\fB\\-%c\\fR",
			CDFlag_flag(flag));
	    }
	    if (writeManArgDesc(out, &ctx, flag) < 0) goto error;
	}
	for (size_t i = 0; i < ctx.nargs; ++i)
	{
	    if (!mdoc) fputs("\n.TP 8n", out);
	    const CliDoc *arg = CDRoot_arg(root, i);
	    ctx.arg = CDArg_arg(arg);
	    fprintf(out, mdoc ? "\n.It .Ar %s" : "\n\\fI%s\\fR", ctx.arg);
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
	    mputs(out, CDText_str(license));
	}
	if (istext(www))
	{
	    if (mdoc) fputs("\n.It WWW:\n.Lk", out);
	    else fputs("\n.TP 10n\nWWW:\n\\fB", out);
	    mputs(out, CDText_str(www));
	    if (!mdoc) fputs("\\fR", out);
	}
	if (mdoc) fputs("\n.El", out);
	else fputs("\n.PD\n.PP", out);
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
	    mnputs(out, astr, es-astr);
	    if (mdoc) fputs(" Aq Mt ", out);
	    else fputs("<\\fI", out);
	    mnputs(out, es+1, ee-es-1);
	    if (!mdoc) fputs("\\fR>", out);
	}
	else fputs(astr, out);
    }

    fputc('\n', out);
    return 0;

error:
    return -1;
}

int writeMan(FILE *out, const CliDoc *root)
{
    return write(out, root, 0);
}

int writeMdoc(FILE *out, const CliDoc *root)
{
    return write(out, root, 1);
}

