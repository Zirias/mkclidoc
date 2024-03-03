#include "clidoc.h"

#include "util.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

C_CLASS_DECL(CDRoot);
C_CLASS_DECL(CDArg);
C_CLASS_DECL(CDFlag);
C_CLASS_DECL(CDList);
C_CLASS_DECL(CDDict);
C_CLASS_DECL(CDTable);
C_CLASS_DECL(CDText);
C_CLASS_DECL(CDDate);

struct CliDoc
{
    CliDoc *parent;
    ContentType type;
};

struct CDRoot
{
    CliDoc base;
    CliDoc *name;
    CliDoc *version;
    CliDoc *comment;
    CliDoc *author;
    CliDoc *license;
    CliDoc *description;
    CliDoc *date;
    CliDoc *www;
    CDFlag **flags;
    CDArg **args;
    size_t nflags;
    size_t nargs;
    int defgroup;
};

struct CDArg
{
    CliDoc base;
    CliDoc *description;
    CliDoc *def;
    CliDoc *min;
    CliDoc *max;
    char *arg;
    int group;
    int optional;
};

struct CDFlag
{
    CDArg base;
    char flag;
};

struct CDList
{
    CliDoc base;
    size_t n;
    CliDoc **c;
};

struct CDDict
{
    CliDoc base;
    size_t n;
    struct {
	char *key;
	CliDoc *val;
    } *v;
};

struct CDTable
{
    CliDoc base;
    size_t width;
    size_t height;
    char **cells;
};

struct CDText
{
    CliDoc base;
    char *text;
};

struct CDDate
{
    CliDoc base;
    time_t date;
};

typedef struct Parser
{
    FILE *doc;
    char *line;
    unsigned long lineno;
    char buf[1024];
} Parser;

static void setoradd(CliDoc **val, CliDoc *item);
static int parsetable(Parser *p, CliDoc **val, CliDoc *parent);
static int parsedict(Parser *p, CliDoc **val, CliDoc *parent);
static int parseval(Parser *p, CliDoc **val, CliDoc *parent);
static int parseint(Parser *p, int *intval);
static int parseargvals(Parser *p, CDArg *arg);
static int parseflag(Parser *p, CDRoot *root);
static int parsearg(Parser *p, CDRoot *root);
static int parse(CDRoot *root, FILE *doc);

#define isws(c) (c == ' ' || c == '\t')
#define skipws(p) while(isws(*(p))) ++(p)
#define skipwsb(p) while(isws(*(p-1))) --(p)
#define err(s) do { \
    fprintf(stderr, "parse error in line %lu: %s\n", p->lineno, (s)); \
    goto error; } while (0)
#define nextline (p->line = ((++p->lineno), \
	    fgets(p->buf, sizeof p->buf, p->doc)))
#define isdict(t) (*p->line == '-' && p->line[1] == ' ' && p->line[2] == '[' \
	&& ((t)=strchr(p->line, ']')) && (t)[1] == ':')
#define istable(t) (*p->line == '|' && ((t)=strchr(p->line+1, '|')) \
	&& strchr((t)+1, '|'))

static void setoradd(CliDoc **val, CliDoc *item)
{
    if (!*val) *val = item;
    else if ((*val)->type == CT_LIST)
    {
	item->parent = *val;
	CDList *list = (CDList *)*val;
	list->c = xrealloc(list->c, (list->n+1) * sizeof *list->c);
	list->c[list->n++] = item;
    }
    else
    {
	CDList *list = xmalloc(sizeof *list);
	list->base.parent = item->parent;
	list->base.type = CT_LIST;
	list->n = 2;
	list->c = xmalloc(2 * sizeof *list->c);
	(*val)->parent = (CliDoc *)list;
	item->parent = (CliDoc *)list;
	list->c[0] = *val;
	list->c[1] = item;
	*val = (CliDoc *)list;
    }
}

static int parsetable(Parser *p, CliDoc **val, CliDoc *parent)
{
    CDTable *table = xmalloc(sizeof *table);
    memset(table, 0, sizeof *table);
    table->base.parent = parent;
    table->base.type = CT_TABLE;

    int width = -1;
    char *tmp = 0;
    while (istable(tmp))
    {
	++p->line;
	tmp = strchr(p->line, '\n');
	if (!tmp) err("Expected end of line");
	if (width > 0)
	{
	    table->cells = xrealloc(table->cells,
		    (table->height + 1) * table->width * sizeof *table->cells);
	}
	for (int x = 0; width < 0 || x < width; ++x)
	{
	    skipws(p->line);
	    tmp = strchr(p->line, '|');
	    if (!tmp)
	    {
		if (width < 0)
		{
		    width = x;
		    break;
		}
		err("Expected table cell");
	    }
	    if (width < 0)
	    {
		table->cells = xrealloc(table->cells,
			++table->width * sizeof *table->cells);
	    }
	    char *end = tmp;
	    skipwsb(end);
	    char *text = 0;
	    size_t txtlen = end > p->line ? end - p->line : 0;
	    if (txtlen)
	    {
		text = xmalloc(txtlen + 1);
		strncpy(text, p->line, txtlen);
		text[txtlen] = 0;
	    }
	    table->cells[table->height*table->width + x] = text;
	    p->line = tmp+1;
	}
	++table->height;
	if (!nextline) err("Unexpected end of file");
	skipws(p->line);
    }
    setoradd(val, (CliDoc *)table);
    return 0;

error:
    CliDoc_destroy((CliDoc *)table);
    return -1;
}

static int parsedict(Parser *p, CliDoc **val, CliDoc *parent)
{
    CDDict *dict = xmalloc(sizeof *dict);
    memset(dict, 0, sizeof *dict);
    dict->base.parent = parent;
    dict->base.type = CT_DICT;

    char *tmp = 0;
    while (isdict(tmp))
    {
	*tmp = 0;
	p->line += 3;
	skipws(p->line);
	if (!*p->line) err("Empty dictionary key");
	char *key = xmalloc(strlen(p->line)+1);
	strcpy(key, p->line);
	p->line = tmp+2;
	skipws(p->line);

	CliDoc *entry = 0;
	if (*p->line == '\n')
	{
	    if (!nextline) err("Unexpected end of file");
	    skipws(p->line);
	    if (istable(tmp))
	    {
		if (parsetable(p, &entry, (CliDoc *)dict) < 0) goto error;
	    }
	}

	if (!entry)
	{
	    size_t txtlen = 0;
	    char *txt = 0;
	    for (;;)
	    {
		if (*p->line == '\n' ||
			(*p->line == '.' && p->line[1] == '\n')) break;
		if (isdict(tmp)) break;
		tmp = strchr(p->line, '\n');
		if (!tmp) err("Expected end of line");
		skipwsb(tmp);
		size_t linelen = tmp - p->line;
		txt = xrealloc(txt, txtlen + linelen + 2);
		strncpy(txt + txtlen, p->line, linelen);
		txt[txtlen + linelen] = ' ';
		txt[txtlen + ++linelen] = 0;
		txtlen += linelen;
		if (!nextline) err("Unexpected end of file");
		skipws(p->line);
	    }
	    if (!txt) err("Empty dictionary value");
	    txt[--txtlen] = 0;

	    CDText *text = xmalloc(sizeof *text);
	    text->base.parent = (CliDoc *)dict;
	    text->base.type = CT_TEXT;
	    text->text = txt;
	    entry = (CliDoc *)text;
	}

	dict->v = xrealloc(dict->v, (dict->n+1) * sizeof *dict->v);
	dict->v[dict->n].key = key;
	dict->v[dict->n++].val = entry;
    }
    setoradd(val, (CliDoc *)dict);
    return 0;

error:
    CliDoc_destroy((CliDoc *)dict);
    return -1;
}

static int parseval(Parser *p, CliDoc **val, CliDoc *parent)
{
    char *tmp;
    char *txt = 0;
    if (*p->line == '\n')
    {
	int done = 0;
	size_t txtlen = 0;
	while (!done) {
	    while (*p->line == '\n')
	    {
		if (!nextline) err("Unexpected end of file");
		skipws(p->line);
	    }
	    int havedict = 0;
	    int havetable = 0;
	    while (*p->line != '\n')
	    {
		if (*p->line == '.' && p->line[1] == '\n')
		{
		    done = 1;
		    break;
		}
		if (isdict(tmp))
		{
		    havedict = 1;
		    break;
		}
		if (istable(tmp))
		{
		    havetable = 1;
		    break;
		}
		tmp = strchr(p->line, '\n');
		if (!tmp) err("Expected end of line");
		skipwsb(tmp);
		*tmp++ = ' ';
		*tmp = 0;
		size_t linelen = strlen(p->line);
		txt = xrealloc(txt, txtlen + linelen + 1);
		strcpy(txt+txtlen, p->line);
		txtlen += linelen;
		if (!nextline) err("Unexpected end of file");
		skipws(p->line);
	    }
	    if (txt)
	    {
		txt[txtlen-1] = 0;
		CDText *text = xmalloc(sizeof *text);
		text->base.parent = parent;
		text->base.type = CT_TEXT;
		text->text = txt;
		txt = 0;
		txtlen = 0;
		setoradd(val, (CliDoc *)text);
	    }
	    if (havedict)
	    {
		if (parsedict(p, val, parent) < 0) goto error;
		if (*p->line == '.' && p->line[1] == '\n') done = 1;
	    }
	    if (havetable)
	    {
		if (parsetable(p, val, parent) < 0) goto error;
		if (*p->line == '.' && p->line[1] == '\n') done = 1;
	    }
	}
    }
    else
    {
	tmp = strchr(p->line, '\n');
	if (!tmp) err("Expected end of line");
	skipwsb(tmp);
	*tmp = 0;
	CDText *text = xmalloc(sizeof *text);
	text->base.parent = parent;
	text->base.type = CT_TEXT;
	text->text = malloc(strlen(p->line)+1);
	strcpy(text->text, p->line);
	*val = (CliDoc *)text;
    }
    p->line = 0;
    return 0;

error:
    free(txt);
    return -1;
}

static int parseint(Parser *p, int *intval)
{
    if (*p->line == '\n') err("Empty value");
    char *tmp = strchr(p->line, '\n');
    if (!tmp) err("Expected end of line");
    skipwsb(tmp);
    if (tmp == p->line) err("Empty value");
    errno = 0;
    char *endp;
    long tmpval = strtol(p->line, &endp, 10);
    if (endp != tmp) err("Non-numeric value");
    if (errno == ERANGE || tmpval < INT_MIN || tmpval > INT_MAX)
    {
	err("Value out of range");
    }
    *intval = tmpval;
    p->line = 0;
    return 0;

error:
    return -1;
}

static int parsedate(Parser *p, CliDoc **val, CliDoc *parent)
{
    struct tm tm = {0};
    char buf[5] = {0};
    char *tmp = strchr(p->line, '\n');
    if (!tmp) err("Expected end of line");
    skipwsb(tmp);
    if (tmp == p->line) err ("Empty value");
    if (tmp - p->line != 8) err ("Expected YYYYMMDD");

    char *endp;
    memcpy(buf, p->line + 6, 2);
    tm.tm_mday = strtol(buf, &endp, 10);
    if (endp != buf + 2) err("Expected YYYYMMDD");
    memcpy(buf, p->line + 4, 2);
    tm.tm_mon = strtol(buf, &endp, 10) - 1;
    if (endp != buf + 2) err("Expected YYYYMMDD");
    memcpy(buf, p->line, 4);
    tm.tm_year = strtol(buf, &endp, 10) - 1900;
    if (endp != buf + 4) err("Expected YYYYMMDD");
    time_t dv = mktime(&tm);
    if (dv == (time_t)(-1)) err("Expected YYYYMMDD");
    p->line = 0;
    CDDate *date = xmalloc(sizeof *date);
    date->base.parent = parent;
    date->base.type = CT_DATE;
    date->date = dv;
    *val = (CliDoc *)date;
    return 0;

error:
    return -1;
}

static int parseargvals(Parser *p, CDArg *arg)
{
    for (;;)
    {
	if (!p->line)
	{
	    if (!nextline) break;
	}
	skipws(p->line);
	if (!*p->line) err("Expected end of line");

	if (*p->line == '\n')
	{
	    p->line = 0;
	    continue;
	}
	if (*p->line == '[')
	{
	    break;
	}
	char *tmp;
	if (isws(*p->line) || *p->line == ':') err("Empty key");
	else tmp = strchr(p->line, ':');
	if (!tmp || tmp == p->line) err("Expected key");
	*tmp = 0;
	CliDoc **val = 0;
	int *intval = 0;
	if (!strcmp(p->line, "description")) val = &arg->description;
	else if (!strcmp(p->line, "default")) val = &arg->def;
	else if (!strcmp(p->line, "min")) val = &arg->min;
	else if (!strcmp(p->line, "max")) val = &arg->max;
	else if (!strcmp(p->line, "group")) intval = &arg->group;
	else if (!strcmp(p->line, "optional")) intval = &arg->optional;
	else err("Unknown key");
	if (val && *val) err("Duplicate key");
	p->line = tmp+1;
	skipws(p->line);
	if (intval)
	{
	    if (parseint(p, intval) < 0) goto error;
	}
	else if (parseval(p, val, (CliDoc *)arg) < 0) goto error;
    }
    return 0;

error:
    return -1;
}

static int parseflag(Parser *p, CDRoot *root)
{
    char *tmp = strchr(p->line, '\n');
    if (!tmp) err("Expected end of line");
    skipwsb(tmp);
    --tmp;
    if (tmp <= p->line) err("Unexpected end of line");
    if (*tmp != ']') err("Tag not closed");
    if (!isws(p->line[1]) && p->line[1] != ']')
    {
	err("Missing or invalid flag character");
    }
    CDFlag *flag = xmalloc(sizeof *flag);
    memset(flag, 0, sizeof *flag);
    flag->base.group = -1;
    flag->base.optional = -1;
    flag->base.base.parent = (CliDoc *)root;
    flag->base.base.type = CT_FLAG;
    root->flags = xrealloc(root->flags,
	    (root->nflags+1) * sizeof *root->flags);
    root->flags[root->nflags++] = flag;
    flag->flag = *p->line++;
    skipws(p->line);
    if (p->line != tmp)
    {
	size_t arglen = tmp - p->line;
	flag->base.arg = xmalloc(arglen + 1);
	strncpy(flag->base.arg, p->line, arglen);
	flag->base.arg[arglen] = 0;
    }
    p->line = 0;
    return parseargvals(p, (CDArg *)flag);

error:
    return -1;
}

static int parsearg(Parser *p, CDRoot *root)
{
    char *tmp = strchr(p->line, '\n');
    if (!tmp) err("Expected end of line");
    skipwsb(tmp);
    --tmp;
    if (tmp <= p->line) err("Unexpected end of line");
    if (*tmp != ']') err("Tag not closed");
    skipws(p->line);
    if (p->line == tmp) err("Missing argument name");
    CDArg *arg = xmalloc(sizeof *arg);
    memset(arg, 0, sizeof *arg);
    arg->group = -1;
    arg->optional = -1;
    arg->base.parent = (CliDoc *)root;
    arg->base.type = CT_ARG;
    root->args = xrealloc(root->args, (root->nargs+1) * sizeof *root->args);
    root->args[root->nargs++] = arg;
    size_t arglen = tmp - p->line;
    arg->arg = xmalloc(arglen + 1);
    strncpy(arg->arg, p->line, arglen);
    arg->arg[arglen] = 0;
    p->line = 0;
    return parseargvals(p, arg);

error:
    return -1;
}

static int parse(CDRoot *root, FILE *doc)
{
    Parser parser = { doc, 0, 0, {0} };
    Parser *p = &parser;

    for (;;)
    {
	if (!p->line)
	{
	    if (!nextline) break;
	}
	skipws(p->line);
	if (!*p->line) err("Expected end of line");

	if (*p->line == '\n')
	{
	    p->line = 0;
	    continue;
	}

	if (*p->line == '[')
	{
	    if (!strncmp(p->line+1, "flag ", 5))
	    {
		p->line += 6;
		if (parseflag(p, root) < 0) goto error;
	    }
	    else if (!strncmp(p->line+1, "arg ", 4))
	    {
		p->line += 5;
		if (parsearg(p, root) < 0) goto error;
	    }
	    else err("Unknown tag");
	    continue;
	}

	char *tmp;
	if (isws(*p->line) || *p->line == ':') err("Empty key");
	else tmp = strchr(p->line, ':');
	if (!tmp || tmp == p->line) err("Expected key");
	*tmp = 0;
	int *intval = 0;
	CliDoc **val = 0;
	CliDoc **dateval = 0;
	if (!strcmp(p->line, "name")) val = &root->name;
	else if (!strcmp(p->line, "version")) val = &root->version;
	else if (!strcmp(p->line, "comment")) val = &root->comment;
	else if (!strcmp(p->line, "author")) val = &root->author;
	else if (!strcmp(p->line, "license")) val = &root->license;
	else if (!strcmp(p->line, "description")) val = &root->description;
	else if (!strcmp(p->line, "date")) dateval = &root->date;
	else if (!strcmp(p->line, "www")) val = &root->www;
	else if (!strcmp(p->line, "defgroup")) intval = &root->defgroup;
	else err("Unknown key");
	if (val && *val) err("Duplicate key");
	if (dateval && *dateval) err("Duplicate key");
	p->line = tmp+1;
	skipws(p->line);
	if (intval)
	{
	    if (parseint(p, intval) < 0) goto error;
	}
	else if (dateval)
	{
	    if (parsedate(p, dateval, (CliDoc *)root) < 0) goto error;
	}
	else if (parseval(p, val, (CliDoc *)root) < 0) goto error;
    }
    return 0;

error:
    return -1;
}

CliDoc *CliDoc_create(FILE *doc)
{
    CDRoot *self = xmalloc(sizeof *self);
    memset(self, 0, sizeof *self);
    self->base.type = CT_ROOT;

    if (parse(self, doc) < 0)
    {
	CliDoc_destroy((CliDoc *)self);
	return 0;
    }
    return (CliDoc *)self;
}

ContentType CliDoc_type(const CliDoc *self)
{
    return self->type;
}

const CliDoc *CliDoc_parent(const CliDoc *self)
{
    return self->parent;
}

const CliDoc *CDRoot_name(const CliDoc *self)
{
    assert(self->type == CT_ROOT);
    return ((const CDRoot *)self)->name;
}

const CliDoc *CDRoot_version(const CliDoc *self)
{
    assert(self->type == CT_ROOT);
    return ((const CDRoot *)self)->version;
}

const CliDoc *CDRoot_comment(const CliDoc *self)
{
    assert(self->type == CT_ROOT);
    return ((const CDRoot *)self)->comment;
}

const CliDoc *CDRoot_author(const CliDoc *self)
{
    assert(self->type == CT_ROOT);
    return ((const CDRoot *)self)->author;
}

const CliDoc *CDRoot_license(const CliDoc *self)
{
    assert(self->type == CT_ROOT);
    return ((const CDRoot *)self)->license;
}

const CliDoc *CDRoot_description(const CliDoc *self)
{
    assert(self->type == CT_ROOT);
    return ((const CDRoot *)self)->description;
}

const CliDoc *CDRoot_date(const CliDoc *self)
{
    assert(self->type == CT_ROOT);
    return ((const CDRoot *)self)->date;
}

const CliDoc *CDRoot_www(const CliDoc *self)
{
    assert(self->type == CT_ROOT);
    return ((const CDRoot *)self)->www;
}

size_t CDRoot_nflags(const CliDoc *self)
{
    assert(self->type == CT_ROOT);
    return ((const CDRoot *)self)->nflags;
}

const CliDoc *CDRoot_flag(const CliDoc *self, size_t i)
{
    assert(i < CDRoot_nflags(self));
    return (const CliDoc *)((const CDRoot *)self)->flags[i];
}

size_t CDRoot_nargs(const CliDoc *self)
{
    assert(self->type == CT_ROOT);
    return ((const CDRoot *)self)->nargs;
}

const CliDoc *CDRoot_arg(const CliDoc *self, size_t i)
{
    assert(i < CDRoot_nargs(self));
    return (const CliDoc *)((const CDRoot *)self)->args[i];
}

int CDRoot_defgroup(const CliDoc *self)
{
    assert(self->type == CT_ROOT);
    return ((const CDRoot *)self)->defgroup;
}

const CliDoc *CDArg_description(const CliDoc *self)
{
    assert(self->type == CT_ARG || self->type == CT_FLAG);
    return ((const CDArg *)self)->description;
}

const CliDoc *CDArg_default(const CliDoc *self)
{
    assert(self->type == CT_ARG || self->type == CT_FLAG);
    return ((const CDArg *)self)->def;
}

const CliDoc *CDArg_min(const CliDoc *self)
{
    assert(self->type == CT_ARG || self->type == CT_FLAG);
    return ((const CDArg *)self)->min;
}

const CliDoc *CDArg_max(const CliDoc *self)
{
    assert(self->type == CT_ARG || self->type == CT_FLAG);
    return ((const CDArg *)self)->max;
}

const char *CDArg_arg(const CliDoc *self)
{
    assert(self->type == CT_ARG || self->type == CT_FLAG);
    return ((const CDArg *)self)->arg;
}

int CDArg_group(const CliDoc *self)
{
    assert(self->type == CT_ARG || self->type == CT_FLAG);
    return ((const CDArg *)self)->group;
}

int CDArg_optional(const CliDoc *self)
{
    assert(self->type == CT_ARG || self->type == CT_FLAG);
    return ((const CDArg *)self)->optional;
}

char CDFlag_flag(const CliDoc *self)
{
    assert(self->type == CT_FLAG);
    return ((const CDFlag *)self)->flag;
}

size_t CDList_length(const CliDoc *self)
{
    assert(self->type == CT_LIST);
    return ((const CDList *)self)->n;
}

const CliDoc *CDList_entry(const CliDoc *self, size_t i)
{
    assert(i < CDList_length(self));
    return ((const CDList *)self)->c[i];
}

size_t CDDict_length(const CliDoc *self)
{
    assert(self->type == CT_DICT);
    return ((const CDDict *)self)->n;
}

const char *CDDict_key(const CliDoc *self, size_t i)
{
    assert(i < CDDict_length(self));
    return ((const CDDict *)self)->v[i].key;
}

const CliDoc *CDDict_val(const CliDoc *self, size_t i)
{
    assert(i < CDDict_length(self));
    return ((const CDDict *)self)->v[i].val;
}

size_t CDTable_width(const CliDoc *self)
{
    assert(self->type == CT_TABLE);
    return ((const CDTable *)self)->width;
}

size_t CDTable_height(const CliDoc *self)
{
    assert(self->type == CT_TABLE);
    return ((const CDTable *)self)->height;
}

const char *CDTable_cell(const CliDoc *self, size_t x, size_t y)
{
    assert(x < CDTable_width(self) && y < CDTable_height(self));
    const CDTable *table = (const CDTable *)self;
    return table->cells[table->width * y + x];
}

const char *CDText_str(const CliDoc *self)
{
    assert(self->type == CT_TEXT);
    return ((const CDText *)self)->text;
}

time_t CDDate_date(const CliDoc *self)
{
    assert(self->type == CT_DATE);
    return ((const CDDate *)self)->date;
}

static void CDText_destroy(CliDoc *self)
{
    CDText *text = (CDText *)self;
    free(text->text);
}

static void CDList_destroy(CliDoc *self)
{
    CDList *list = (CDList *)self;
    for (size_t i = 0; i < list->n; ++i)
    {
	CliDoc_destroy(list->c[i]);
    }
    free(list->c);
}

static void CDDict_destroy(CliDoc *self)
{
    CDDict *dict = (CDDict *)self;
    for (size_t i = 0; i < dict->n; ++i)
    {
	free(dict->v[i].key);
	CliDoc_destroy(dict->v[i].val);
    }
    free(dict->v);
}

static void CDTable_destroy(CliDoc *self)
{
    CDTable *table = (CDTable *)self;
    for (size_t i = 0; i < table->width * table->height; ++i)
    {
	free(table->cells[i]);
    }
    free(table->cells);
}

static void CDArg_destroy(CliDoc *self)
{
    CDArg *arg = (CDArg *)self;
    CliDoc_destroy(arg->description);
    CliDoc_destroy(arg->def);
    CliDoc_destroy(arg->min);
    CliDoc_destroy(arg->max);
    free(arg->arg);
}

static void CDRoot_destroy(CliDoc *self)
{
    CDRoot *root = (CDRoot *)self;
    CliDoc_destroy(root->name);
    CliDoc_destroy(root->version);
    CliDoc_destroy(root->comment);
    CliDoc_destroy(root->author);
    CliDoc_destroy(root->license);
    CliDoc_destroy(root->description);
    CliDoc_destroy(root->date);
    CliDoc_destroy(root->www);
    if (root->nflags)
    {
	for (size_t i = 0; i < root->nflags; ++i)
	{
	    CliDoc_destroy((CliDoc *)root->flags[i]);
	}
	free(root->flags);
    }
    if (root->nargs)
    {
	for (size_t i = 0; i < root->nargs; ++i)
	{
	    CliDoc_destroy((CliDoc *)root->args[i]);
	}
	free(root->args);
    }
}

void CliDoc_destroy(CliDoc *self)
{
    if (!self) return;
    switch (self->type)
    {
	case CT_ROOT: CDRoot_destroy(self); break;
	case CT_ARG:
	case CT_FLAG: CDArg_destroy(self); break;
	case CT_LIST: CDList_destroy(self); break;
	case CT_DICT: CDDict_destroy(self); break;
	case CT_TABLE: CDTable_destroy(self); break;
	case CT_TEXT: CDText_destroy(self); break;
	case CT_DATE:
	default: break;
    }
    free(self);
}

