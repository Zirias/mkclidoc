#include "cppwriter.h"

#include "clidoc.h"
#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Ctx
{
    const char *name;
    const char *arg;
    int first;
} Ctx;

static void cpputc(FILE *out, int c)
{
    if (c == '\\' || c == '"') fputc('\\', out);
    fputc(c, out);
}

static void skipws(const char **str)
{
    while (**str == ' ' || **str == '\t') ++(*str);
}

static const char *findstrpos(const char *str, const char *pat, size_t len)
{
    size_t slen = 0;
    while (slen < len && str[slen]) ++slen;
    size_t plen = strlen(pat);
    if (plen > slen) return 0;
    for (size_t i = 0; i <= slen - plen; ++i)
    {
	if (!strncmp(pat, str+i, plen)) return str+i;
    }
    return 0;
}

static void writeCppStrLine(FILE *out, const Ctx *ctx,
	const char **str, int indent)
{
    int len = 0;
    skipws(str);
    if (!**str) return;
    if (indent < 0)
    {
	indent = -indent;
    }
    else
    {
	fputs("\\n\" \\\n\"", out);
	if (indent) fprintf(out, "%*s", indent, "");
    }
    while (**str)
    {
	char wsbuf[78] = {0};
	char *wsp = wsbuf;
	while (len + indent < 78 && (**str == ' ' || **str == '\t'))
	{
	    *wsp++ = *(*str)++;
	    ++len;
	}
	size_t wlen = strcspn(*str, " \t");
	size_t olen;
	size_t rlen;
	size_t plen;
	const char *repl = 0;
	const char *token = 0;
	if (wlen >= 8 && (token = findstrpos(*str, "%%name%%", wlen)))
	{
	    repl = ctx->name;
	    rlen = 8;
	    olen = wlen - rlen + strlen(repl);
	    plen = token - *str;
	    wlen -= (rlen + plen);
	}
	else if (wlen >= 7 && (token = findstrpos(*str, "%%arg%%", wlen)))
	{
	    repl = ctx->arg;
	    rlen = 7;
	    olen = wlen - rlen + strlen(repl);
	    plen = token - *str;
	    wlen -= (rlen + plen);
	}
	else olen = wlen;
	if (!len || len + indent + olen < 78)
	{
	    wsp = wsbuf;
	    while (*wsp) fputc(*wsp++, out);
	    if (repl)
	    {
		for (size_t i = 0; i < plen; ++i) cpputc(out, (*str)[i]);
		*str += rlen + plen;
		while (*repl) cpputc(out, *repl++);
	    }
	    for (size_t i = 0; i < wlen; ++i) cpputc(out, (*str)[i]);
	    *str += wlen;
	    len += olen;
	}
	else break;
    }
}

static int writeUsageFlag(FILE *out, int pos, const char *flag, int optional)
{
    size_t len = strlen(flag);
    if (optional) len += 2;
    if (pos > 12 && pos + len >= 78)
    {
	fputs("\\n\" \\\n\"            ", out);
	pos = 12;
    }
    else
    {
	++len;
	fputc(' ', out);
    }
    if (optional) fputc('[', out);
    while (*flag) cpputc(out, *flag++);
    if (optional) fputc(']', out);
    return pos + len;
}

#define err(m) do { \
    fprintf(stderr, "Cannot write cpp: %s\n", (m)); goto error; } while (0)
#define istext(m) ((m) && CliDoc_type(m) == CT_TEXT)

static void writeDescription(FILE *out, Ctx *ctx,
	const CliDoc *desc, int indent);

static void writeList(FILE *out, Ctx *ctx,
	const CliDoc *list, int indent)
{
    size_t len = CDList_length(list);
    for (size_t i = 0; i < len; ++i)
    {
	writeDescription(out, ctx, CDList_entry(list, i), indent);
    }
}

static void writeDict(FILE *out, Ctx *ctx,
	const CliDoc *dict, int indent)
{
    int subindent = 0;
    size_t len = CDDict_length(dict);
    for (size_t i = 0; i < len; ++i)
    {
	size_t keywidth = strlen(CDDict_key(dict, i)) + 2;
	if (keywidth > (size_t)subindent) subindent = keywidth;
    }
    for (size_t i = 0; i < len; ++i)
    {
	fprintf(out, "\\n\" \\\n\"%*s%-*s", indent, "", subindent,
		CDDict_key(dict, i));
	ctx->first = 1;
	writeDescription(out, ctx, CDDict_val(dict, i), indent + subindent);
    }
}

static void writeTable(FILE *out, Ctx *ctx,
	const CliDoc *table, int indent)
{
    size_t width = CDTable_width(table);
    size_t height = CDTable_height(table);
    int colpos[32] = {0};
    for (size_t y = 0; y < height; ++y)
    {
	for (size_t x = 0; x < width && x < 32; ++x)
	{
	    size_t cw = strlen(CDTable_cell(table, x, y));
	    if (cw > (size_t)colpos[x]) colpos[x] = cw;
	}
    }
    int pos = colpos[0] + 2;
    for (size_t x = 0; x < width && x < 32;)
    {
	colpos[x] = pos;
	pos += colpos[++x] + 2;
    }
    for (size_t y = 0; y < height; ++y)
    {
	if (!ctx->first) fprintf(out, "\\n\" \\\n\"%*s", indent, "");
	ctx->first = 0;
	pos = 0;
	for (size_t x = 0; x < width; ++x)
	{
	    const char *cell = CDTable_cell(table, x, y);
	    while (*cell)
	    {
		if (!strncmp(cell, "%%name%%", 8))
		{
		    const char *repl = ctx->name;
		    while (*repl)
		    {
			cpputc(out, *repl++);
			++pos;
		    }
		    cell += 8;
		}
		else if (!strncmp(cell, "%%arg%%", 7))
		{
		    const char *repl = ctx->arg;
		    while (*repl)
		    {
			cpputc(out, *repl++);
			++pos;
		    }
		    cell += 8;
		}
		else
		{
		    cpputc(out, *cell++);
		    ++pos;
		}
	    }
	    if (x < width - 1) while (pos < colpos[x])
	    {
		fputc(' ', out);
		++pos;
	    }
	}
    }
}

static void writeDescription(FILE *out, Ctx *ctx,
	const CliDoc *desc, int indent)
{
    const char *str;
    switch (CliDoc_type(desc))
    {
	case CT_TEXT:
	    str = CDText_str(desc);
	    while (*str)
	    {
		writeCppStrLine(out, ctx, &str, (ctx->first?-1:1) * indent);
		ctx->first = 0;
	    }
	    break;

	case CT_LIST:
	    writeList(out, ctx, desc, indent);
	    break;

	case CT_DICT:
	    writeDict(out, ctx, desc, indent);
	    break;

	case CT_TABLE:
	    writeTable(out, ctx, desc, indent);
	    break;

	default:
	    break;
    }
}

static void writeArgDesc(FILE *out, Ctx *ctx,
	const CliDoc *arg, int indent)
{
    writeDescription(out, ctx, CDArg_description(arg), indent);
    const CliDoc *min = CDArg_min(arg);
    const CliDoc *max = CDArg_max(arg);
    const CliDoc *def = CDArg_default(arg);
    if (istext(min) || istext(max) || istext(def))
    {
	const char *minstr = 0;
	size_t mmdlen = 0;
	if (istext(min))
	{
	    minstr = CDText_str(min);
	    mmdlen += strlen(minstr) + 5;
	}
	const char *maxstr = 0;
	if (istext(max))
	{
	    maxstr = CDText_str(max);
	    if (mmdlen) mmdlen += 2;
	    mmdlen += strlen(maxstr) + 5;
	}
	const char *defstr = 0;
	if (istext(def))
	{
	    defstr = CDText_str(def);
	    if (mmdlen) mmdlen += 2;
	    mmdlen += strlen(defstr) + 9;
	}
	char *mmd = xmalloc(mmdlen + 1);
	*mmd = 0;
	if (minstr)
	{
	    strcat(mmd, "min: ");
	    strcat(mmd, minstr);
	}
	if (maxstr)
	{
	    if (*mmd) strcat(mmd, ", max: ");
	    else strcat(mmd, "max: ");
	    strcat(mmd, maxstr);
	}
	if (defstr)
	{
	    if (*mmd) strcat(mmd, ", default: ");
	    else strcat(mmd, "default: ");
	    strcat(mmd, defstr);
	}
	const char *str = mmd;
	while (*str)
	{
	    writeCppStrLine(out, ctx, &str, (ctx->first?-1:1) * indent);
	    ctx->first = 0;
	}
	free(mmd);
    }
}

int writeCpp(FILE *out, const CliDoc *root)
{
    assert(CliDoc_type(root) == CT_ROOT);

    const CliDoc *name = CDRoot_name(root);
    if (!istext(name)) err("missing name");
    const char *namestr = CDText_str(name);
    char ucname[64];
    int i;
    for (i = 0; i < 63 && namestr[i]; ++i)
    {
	ucname[i] = toupper(namestr[i]);
    }
    ucname[i] = 0;
    int usagewidth = i;
    if (usagewidth < 32) usagewidth = 32;

    fprintf(out, "#ifndef %s_HELP\n\n#undef %s_USAGE_FMT\n"
	    "#undef %s_USAGE_ARGS\n\n#define %s_USAGE_FMT \\\n\"Usage: %%s",
	    ucname, ucname, ucname, ucname);

    int nflags = CDRoot_nflags(root);
    int nargs = CDRoot_nargs(root);
    int indent = 0;
    char flagstr[256];

    if (nflags + nargs > 0)
    {
	int defgroup = CDRoot_defgroup(root);
	int n;
	for (i = 0, n = 0; i <= defgroup || n; ++i, n = 0)
	{
	    char noarg[128] = {0};
	    char rnoarg[128] = {0};
	    int nalen = 0;
	    int rnalen = 0;
	    for (int j = 0; j < nflags; ++j)
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
	    int pos = usagewidth;
	    if (i) fputs ("\\n\" \\\n\"       %s", out);
	    if (rnalen)
	    {
		sprintf(flagstr, "-%s", rnoarg);
		pos = writeUsageFlag(out, pos, flagstr, 0);
		++n;
	    }
	    if (nalen)
	    {
		sprintf(flagstr, "-%s", noarg);
		pos = writeUsageFlag(out, pos, flagstr, 1);
		++n;
	    }
	    for (int j = 0; j < nflags; ++j)
	    {
		const CliDoc *flag = CDRoot_flag(root, j);
		int group = CDFlag_group(flag);
		if (group < 0) group = defgroup;
		if (group < 0) group = 0;
		if (group != i) continue;
		const char *arg = CDFlag_arg(flag);
		if (!arg) continue;
		size_t arglen = strlen(arg);
		if (arglen > 80) err("argument too long");
		if (arglen + 3 > (size_t)indent) indent = arglen + 3;
		int optional = CDFlag_optional(flag);
		sprintf(flagstr, "-%c %s", CDFlag_flag(flag), arg);
		pos = writeUsageFlag(out, pos, flagstr, optional);
	    }
	    for (int j = 0; j < nargs; ++j)
	    {
		const CliDoc *arg = CDRoot_arg(root, j);
		int group = CDArg_group(arg);
		if (group < 0) group = defgroup;
		if (group < 0) group = 0;
		if (group != i) continue;
		const char *argstr = CDArg_arg(arg);
		size_t arglen = strlen(argstr);
		if (arglen > 80) err("argument too long");
		if (arglen > (size_t)indent) indent = arglen;
		int optional = CDArg_optional(arg);
		pos = writeUsageFlag(out, pos, argstr, optional);
	    }
	}
    }

    fprintf(out, "\\n\"\n\n#define %s_USAGE_ARGS(argv0)", ucname);
    for (int j = 0; j < i; ++j)
    {
	fputc(j ? ',' : ' ', out);
	fputs("(argv0)", out);
    }

    fprintf(out, "\n\n#define %s_HELP", ucname);

    if (nflags + nargs > 0)
    {
	Ctx ctx = { namestr, 0, 0 };
	indent += 2;
	fputs(" \"", out);
	for (i = 0; i < nflags; ++i)
	{
	    const CliDoc *flag = CDRoot_flag(root, i);
	    const char *arg = CDFlag_arg(flag);
	    if (arg) sprintf(flagstr, "-%c %s", CDFlag_flag(flag), arg);
	    else sprintf(flagstr, "-%c", CDFlag_flag(flag));
	    fprintf(out, "\\n\" \\\n\"    %-*s", indent, flagstr);
	    ctx.arg = arg;
	    ctx.first = 1;
	    writeArgDesc(out, &ctx, flag, indent + 4);
	}
	for (i = 0; i < nargs; ++i)
	{
	    const CliDoc *arg = CDRoot_arg(root, i);
	    fprintf(out, "\\n\" \\\n\"    %-*s", indent, CDArg_arg(arg));
	    ctx.arg = CDArg_arg(arg);
	    ctx.first = 1;
	    writeArgDesc(out, &ctx, arg, indent + 4);
	}
	fputs("\\n\"", out);
    }

    fputs("\n\n#endif\n", out);
    return 0;

error:
    return -1;
}

