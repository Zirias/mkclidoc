#include "mdocwriter.h"

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
} Ctx;
#define Ctx_init() {0, 0, 0, 0}

#define err(m) do { \
    fprintf(stderr, "Cannot write mdoc: %s\n", (m)); goto error; } while (0)
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

static void writeMdocText(FILE *out, Ctx *ctx, const char *str)
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
	    fputs(".Nm", out);
	    str += 8;
	    if (*str)
	    {
		if (*str != ' ')
		{
		    if (*str != '.' && *str != ','
			    && *str != ':' && *str != ';')
		    {
			fputs(" Ns No ", out);
		    }
		    else fputc(' ', out);
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
	    fprintf(out, ".Ar %s", ctx->arg);
	    str += 7;
	    if (*str)
	    {
		if (*str != ' ')
		{
		    if (*str != '.' && *str != ','
			    && *str != ':' && *str != ';')
		    {
			fputs(" Ns No ", out);
		    }
		    else fputc(' ', out);
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

static void writeMdocField(FILE *out, Ctx *ctx,
	const char *key, const char *val)
{
    fprintf(out, ".%s ", key);
    writeMdocText(out, ctx, val);
    fputc('\n', out);
}

static int writeMdocSynopsis(FILE *out, Ctx *ctx, const CliDoc *root)
{
    fprintf(out, ".Sh SYNOPSIS\n");

    ctx->nflags = CDRoot_nflags(root);
    ctx->nargs = CDRoot_nargs(root);

    if (ctx->nflags + ctx->nargs == 0)
    {
	fprintf(out, ".Nm\n");
    }
    else
    {
	int defgroup = CDRoot_defgroup(root);
	for (int i = 0, n = 0; i <= defgroup || n; ++i, n = 0)
	{
	    fprintf(out, ".Nm\n");
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
		fprintf(out, ".Fl %s\n", rnoarg);
		++n;
	    }
	    if (nalen)
	    {
		fprintf(out, ".Op Fl %s\n", noarg);
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
		    fprintf(out, ".Fl %c Ar %s\n", CDFlag_flag(flag), arg);
		}
		else
		{
		    fprintf(out, ".Op Fl %c Ar %s\n", CDFlag_flag(flag), arg);
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
		    fprintf(out, ".Op Ar %s\n", CDArg_arg(arg));
		}
		else
		{
		    fprintf(out, ".Ar %s\n", CDArg_arg(arg));
		}
		++n;
	    }
	}
    }
    return 0;

error:
    return -1;
}

static int writeMdocDescription(FILE *out, Ctx *ctx,
	const CliDoc *desc, int idx);

static int writeMdocList(FILE *out, Ctx *ctx, const CliDoc *list)
{
    size_t len = CDList_length(list);
    for (size_t i = 0; i < len; ++i)
    {
	const CliDoc *entry = CDList_entry(list, i);
	if (writeMdocDescription(out, ctx, entry, i) < 0) goto error;
    }
    return 0;

error:
    return -1;
}

static void writeMdocCell(FILE *out, Ctx *ctx, const char *cell)
{
    if (!cell) return;
    while (*cell) switch (*cell)
    {
	case '%':
	    if (!strcmp(cell, "%%name%%"))
	    {
		fprintf(out, "\" Ns Nm %s \"", ctx->name);
		cell += 8;
		continue;
	    }
	    if (!strcmp(cell, "%%arg%%"))
	    {
		fprintf(out, "\" Ns Ar %s \"", ctx->arg);
		cell += 7;
		continue;
	    }
	    ATTR_FALLTHROUGH;
	case '"':
	case '\\':
	    fputc('\\', out);
	    ATTR_FALLTHROUGH;
	default:
	    fputc(*cell++, out);
    }
}

static void writeMdocTable(FILE *out, Ctx *ctx, const CliDoc *table)
{
    struct {
	size_t len;
	const char *str;
    } *wspec = 0;
    size_t width = CDTable_width(table);
    size_t height = CDTable_height(table);
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
	fprintf(out, " \"%s\"", wspec[x].str);
    }
    free(wspec);
    for (size_t y = 0; y < height; ++y)
    {
	for (size_t x = 0; x < width; ++x)
	{
	    fputs(x ? " Ta \"" : "\n.It \"", out);
	    writeMdocCell(out, ctx, CDTable_cell(table, x, y));
	    fputc('"', out);
	}
    }
    fputs("\n.El", out);
}

static int writeMdocDict(FILE *out, Ctx *ctx, const CliDoc *dict)
{
    fputs("\n.Bl -tag -width Ds -compact", out);
    size_t len = CDDict_length(dict);
    for (size_t i = 0; i < len; ++i)
    {
	const char *key = CDDict_key(dict, i);
	const CliDoc *val = CDDict_val(dict, i);
	fprintf(out, "\n.It %s", key);
	if (writeMdocDescription(out, ctx, val, 0) < 0) return -1;
    }
    fputs("\n.El", out);
    return 0;
}

static int writeMdocDescription(FILE *out, Ctx *ctx,
	const CliDoc *desc, int idx)
{
    if (!desc) return 0;
    switch (CliDoc_type(desc))
    {
	case CT_TEXT:
	    if (idx) fputs("\n.sp", out);
	    fputc('\n', out);
	    writeMdocText(out, ctx, CDText_str(desc));
	    break;
	
	case CT_DICT:
	    if (idx) fputs("\n.sp", out);
	    if (writeMdocDict(out, ctx, desc) < 0) goto error;
	    break;

	case CT_TABLE:
	    if (idx) fputs("\n.sp", out);
	    writeMdocTable(out, ctx, desc);
	    break;

	case CT_LIST:
	    if (writeMdocList(out, ctx, desc) < 0) goto error;
	    break;

	default:
	    break;
    }
    return 0;

error:
    return -1;
}

static int writeMdocArgDesc(FILE *out, Ctx *ctx, const CliDoc *arg)
{
    if (writeMdocDescription(out, ctx,
		CDArg_description(arg), 0) < 0) return -1;
    const CliDoc *min = CDArg_min(arg);
    const CliDoc *max = CDArg_max(arg);
    const CliDoc *def = CDArg_default(arg);
    if (min || max || def)
    {
	fputs("\n.sp\n.Bl -tag -width default: -compact", out);
	if (min)
	{
	    fputs("\n.It min:", out);
	    if (writeMdocDescription(out, ctx, min, 0) < 0) return -1;
	}
	if (max)
	{
	    fputs("\n.It max:", out);
	    if (writeMdocDescription(out, ctx, max, 0) < 0) return -1;
	}
	if (def)
	{
	    fputs("\n.It default:", out);
	    if (writeMdocDescription(out, ctx, def, 0) < 0) return -1;
	}
	fputs("\n.El", out);
    }
    return 0;
}

int writeMdoc(FILE *out, const CliDoc *root)
{
    assert(CliDoc_type(root) == CT_ROOT);
    
    Ctx ctx = Ctx_init();

    const CliDoc *date = CDRoot_date(root);
    if (!date || CliDoc_type(date) != CT_DATE) err("missing date");
    const CliDoc *name = CDRoot_name(root);
    if (!istext(name)) err("missing name");
    ctx.name = CDText_str(name);
    const CliDoc *comment = CDRoot_comment(root);
    if (!istext(comment)) err("missing comment");

    time_t dv = CDDate_date(date);
    struct tm *tm = localtime(&dv);
    strftime(strbuf, sizeof strbuf, "%B %e, %Y", tm);
    writeMdocField(out, &ctx, "Dd", strbuf);
    fprintf(out, ".Dt %s 1\n.Os\n", strToUpper(ctx.name));
    fputs(".Sh NAME\n", out);
    writeMdocField(out, &ctx, "Nm", ctx.name);
    writeMdocField(out, &ctx, "Nd", CDText_str(comment));

    if (writeMdocSynopsis(out, &ctx, root) < 0) goto error;

    fputs(".Sh DESCRIPTION", out);
    if (writeMdocDescription(out, &ctx,
		CDRoot_description(root), 0) < 0) goto error;

    if (ctx.nflags + ctx.nargs > 0)
    {
	fputs("\n.sp\nThe options are as follows:\n.Bl -tag -width Ds", out);
	for (size_t i = 0; i < ctx.nflags; ++i)
	{
	    const CliDoc *flag = CDRoot_flag(root, i);
	    const char *arg = CDFlag_arg(flag);
	    if (arg)
	    {
		ctx.arg = arg;
		fprintf(out, "\n.It Fl %c Ar %s", CDFlag_flag(flag), arg);
	    }
	    else
	    {
		ctx.arg = 0;
		fprintf(out, "\n.It Fl %c", CDFlag_flag(flag));
	    }
	    if (writeMdocArgDesc(out, &ctx, flag) < 0) goto error;
	}
	for (size_t i = 0; i < ctx.nargs; ++i)
	{
	    const CliDoc *arg = CDRoot_arg(root, i);
	    ctx.arg = CDArg_arg(arg);
	    fprintf(out, "\n.It Ar %s", ctx.arg);
	    if (writeMdocArgDesc(out, &ctx, arg) < 0) goto error;
	}
	fputs("\n.El", out);
    }

    const CliDoc *version = CDRoot_version(root);
    const CliDoc *license = CDRoot_license(root);
    const CliDoc *www = CDRoot_www(root);
    if (istext(version) || istext(license) || istext(www))
    {
	fputs("\n.Ss Additional information\n"
		".Bl -tag -width Version: -compact", out);
	if (istext(version))
	{
	    fputs("\n.It Version:\n.Nm\n", out);
	    mputs(out, CDText_str(version));
	}
	if (istext(license))
	{
	    fputs("\n.It License:\n", out);
	    mputs(out, CDText_str(license));
	}
	if (istext(www))
	{
	    fputs("\n.It WWW:\n.Lk ", out);
	    mputs(out, CDText_str(www));
	}
	fputs("\n.El", out);
    }

    const CliDoc *author = CDRoot_author(root);
    if (istext(author))
    {
	fputs("\n.Sh AUTHORS\n.An ", out);
	const char *astr = CDText_str(author);
	const char *es = strchr(astr, '<');
	const char *ea = strchr(astr, '@');
	const char *ee = strchr(astr, '>');
	if (es && ea && ee && es < ea && ea < ee)
	{
	    mnputs(out, astr, es-astr);
	    fputs(" Aq Mt ", out);
	    mnputs(out, es+1, ee-es-1);
	}
	else fputs(astr, out);
    }

    fputc('\n', out);
    return 0;

error:
    return -1;
}
