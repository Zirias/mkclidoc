#include "clidoc.h"

#include "util.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONTENT_FIELDS \
    CDContent *parent; \
    ContentType type

struct CDContent
{
    CONTENT_FIELDS;
};

struct CliDoc
{
    CONTENT_FIELDS;
    CDContent *name;
    CDContent *version;
    CDContent *author;
    CDContent *license;
    CDContent *description;
    CDFlag **flags;
    CDArg **args;
    size_t nflags;
    size_t nargs;
    int defgroup;
};

struct CDList
{
    CONTENT_FIELDS;
    size_t n;
    CDContent **c;
};

struct CDText
{
    CONTENT_FIELDS;
    char *text;
};

#define ARG_FIELDS \
    CONTENT_FIELDS; \
    CDContent *description; \
    CDContent *def; \
    CDContent *min; \
    CDContent *max; \
    char *arg; \
    int group; \
    int optional

struct CDArg
{
    ARG_FIELDS;
};

struct CDFlag
{
    ARG_FIELDS;
    char flag;
};

typedef enum ParseState
{
    PS_ROOT,
    PS_ARGVALS,
    PS_VAL,
    PS_INTVAL,
    PS_FLAG,
    PS_ARG
} ParseState;

#define isws(c) (c == ' ' || c == '\t')
#define skipws(p) while(isws(*(p))) ++(p)
#define skipwsb(p) while(isws(*(p-1))) --(p)
#define err(s) do { \
    fprintf(stderr, "parse error in line %lu: %s\n", lineno, (s)); \
    goto error; } while (0)
#define nextline (line = ((++lineno),fgets(buf, sizeof buf, doc)))

CliDoc *CliDoc_create(FILE *doc)
{
    CliDoc *self = xmalloc(sizeof *self);
    memset(self, 0, sizeof *self);
    self->type = CT_ROOT;

    CDContent *parent = (CDContent *)self;
    CDContent **val;
    CDArg *arg;
    CDFlag *flag;
    int *intval;
    ParseState state = PS_ROOT;

    char buf[1024];
    char name[32];
    size_t namelen;
    char *line = 0;
    char *tmp;
    char *endp;
    long tmpval;
    unsigned long lineno = 0;
    for (;;)
    {
	if (!line)
	{
	    if (!nextline) break;
	}
	skipws(line);
	if (!*line) err("Expected end of line");

	switch (state)
	{
	    case PS_ROOT:
		if (*line == '\n')
		{
		    line = 0;
		    break;
		}
		if (*line == '[')
		{
		    if (!strncmp(line+1, "flag ", 5))
		    {
			line += 6;
			state = PS_FLAG;
		    }
		    else if (!strncmp(line+1, "arg ", 4))
		    {
			line += 5;
			state = PS_ARG;
		    }
		    break;
		}
		else if (isws(*line) || *line == ':') err("Empty key");
		else tmp = strchr(line, ':');
		if (!tmp || tmp == line) err("Expected key");
		namelen = tmp - line;
		if (namelen > 31) err("Key too long");
		strncpy(name, line, namelen);
		name[namelen] = 0;
		val = 0;
		intval = 0;
		if (!strcmp(name, "name")) val = &self->name;
		else if (!strcmp(name, "version")) val = &self->version;
		else if (!strcmp(name, "author")) val = &self->author;
		else if (!strcmp(name, "license")) val = &self->license;
		else if (!strcmp(name, "description")) val = &self->description;
		else if (!strcmp(name, "defgroup")) intval = &self->defgroup;
		else err("Unknown key");
		if (val && *val) err("Duplicate key");
		line = tmp+1;
		state = val ? PS_VAL : PS_INTVAL;
		break;

	    case PS_ARGVALS:
		if (*line == '\n')
		{
		    line = 0;
		    break;
		}
		if (*line == '[')
		{
		    parent = arg->parent;
		    goto parent;
		}
		else if (isws(*line) || *line == ':') err("Empty key");
		else tmp = strchr(line, ':');
		if (!tmp || tmp == line) err("Expected key");
		namelen = tmp - line;
		if (namelen > 31) err("Key too long");
		strncpy(name, line, namelen);
		name[namelen] = 0;
		val = 0;
		intval = 0;
		if (!strcmp(name, "description")) val = &arg->description;
		else if (!strcmp(name, "default")) val = &arg->def;
		else if (!strcmp(name, "min")) val = &arg->min;
		else if (!strcmp(name, "max")) val = &arg->max;
		else if (!strcmp(name, "group")) intval = &arg->group;
		else if (!strcmp(name, "optional")) intval = &arg->optional;
		else err("Unknown key");
		if (val && *val) err("Duplicate key");
		line = tmp+1;
		state = val ? PS_VAL : PS_INTVAL;
		break;

	    case PS_VAL:
		if (*line == '\n')
		{
		    int done = 0;
		    char *txt = 0;
		    size_t txtlen = 0;
		    for (;;) {
			if (!nextline) err("Unexpected end of file");
			skipws(line);
			if (*line == '\n') continue;
			while (!done && *line != '\n')
			{
			    if (*line == '.' && line[1] == '\n')
			    {
				done = 1;
				break;
			    }
			    tmp = strchr(line, '\n');
			    if (!tmp) err("Expected end of line");
			    skipwsb(tmp);
			    *tmp++ = ' ';
			    *tmp = 0;
			    size_t linelen = strlen(line);
			    txt = xrealloc(txt, txtlen + linelen + 1);
			    strcpy(txt+txtlen, line);
			    txtlen += linelen;
			    if (!nextline) err("Unexpected end of file");
			    skipws(line);
			}
			txt[txtlen-1] = 0;
			CDText *text = xmalloc(sizeof *text);
			text->parent = parent;
			text->type = CT_TEXT;
			text->text = txt;
			txt = 0;
			txtlen = 0;
			if (!*val) *val = (CDContent *)text;
			else if ((*val)->type == CT_LIST)
			{
			    text->parent = *val;
			    CDList *list = (CDList *)*val;
			    list->c = xrealloc(list->c,
				    (list->n+1) * sizeof *list->c);
			    list->c[list->n++] = (CDContent *)text;
			}
			else
			{
			    CDText *first = (CDText *)*val;
			    CDList *list = xmalloc(sizeof *list);
			    list->parent = parent;
			    list->type = CT_LIST;
			    list->n = 2;
			    list->c = xmalloc(2 * sizeof *list->c);
			    first->parent = (CDContent *)list;
			    text->parent = (CDContent *)list;
			    list->c[0] = (CDContent *)first;
			    list->c[1] = (CDContent *)text;
			    *val = (CDContent *)list;
			}
			if (done) break;
		    }
		}
		else
		{
		    tmp = strchr(line, '\n');
		    if (!tmp) err("Expected end of line");
		    skipwsb(tmp);
		    *tmp = 0;
		    CDText *text = xmalloc(sizeof *text);
		    text->parent = parent;
		    text->type = CT_TEXT;
		    text->text = malloc(strlen(line)+1);
		    strcpy(text->text, line);
		    *val = (CDContent *)text;
		}
		line = 0;
		goto parent;

	    case PS_INTVAL:
		if (*line == '\n') err("Empty value");
		tmp = strchr(line, '\n');
		if (!tmp) err("Expected end of line");
		skipwsb(tmp);
		if (tmp == line) err("Empty value");
		errno = 0;
		tmpval = strtol(line, &endp, 10);
		if (endp != tmp) err("Non-numeric value");
		if (errno == ERANGE || tmpval < INT_MIN || tmpval > INT_MAX)
		{
		    err("Value out of range");
		}
		*intval = tmpval;
		line = 0;
		goto parent;

	    case PS_FLAG:
		tmp = strchr(line, '\n');
		if (!tmp) err("Expected end of line");
		skipwsb(tmp);
		--tmp;
		if (tmp <= line) err("Unexpected end of line");
		if (*tmp != ']') err("Missing flag character");
		if (!isws(line[1]) && line[1] != ']')
		{
		    err("Invalid flag character");
		}
		flag = xmalloc(sizeof *flag);
		memset(flag, 0, sizeof *flag);
		flag->group = -1;
		flag->optional = -1;
		flag->parent = parent;
		flag->type = CT_FLAG;
		self->flags = xrealloc(self->flags,
			(self->nflags+1) * sizeof *self->flags);
		self->flags[self->nflags++] = flag;
		flag->flag = *line++;
		skipws(line);
		if (line != tmp)
		{
		    size_t arglen = tmp - line;
		    flag->arg = xmalloc(arglen + 1);
		    strncpy(flag->arg, line, arglen);
		    flag->arg[arglen] = 0;
		}
		parent = (CDContent *)flag;
		arg = (CDArg *)flag;
		state = PS_ARGVALS;
		line = 0;
		break;

	    case PS_ARG:
		tmp = strchr(line, '\n');
		if (!tmp) err("Expected end of line");
		skipwsb(tmp);
		--tmp;
		if (tmp <= line) err("Unexpected end of line");
		if (*tmp != ']') err("Missing argument name");
		arg = xmalloc(sizeof *arg);
		memset(arg, 0, sizeof *arg);
		arg->group = -1;
		arg->optional = -1;
		arg->parent = parent;
		arg->type = CT_ARG;
		self->args = xrealloc(self->args,
			(self->nargs+1) * sizeof *self->args);
		self->args[self->nargs++] = arg;
		skipws(line);
		if (line == tmp) err("Missing argument name");
		size_t arglen = tmp - line;
		arg->arg = xmalloc(arglen + 1);
		strncpy(arg->arg, line, arglen);
		arg->arg[arglen] = 0;
		parent = (CDContent *)arg;
		state = PS_ARGVALS;
		line = 0;
		break;

	    parent:
		switch (parent->type)
		{
		    case CT_ARG:
		    case CT_FLAG:
			state = PS_ARGVALS;
			break;
		    default:
			state = PS_ROOT;
			break;
		}
		break;
	}
    }

    return self;

error:
    CliDoc_destroy(self);
    return 0;
}

void CliDoc_destroy(CliDoc *self)
{
    free(self);
}

