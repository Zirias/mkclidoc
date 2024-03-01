#include "clidoc.h"

#include "util.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct CDContent
{
    CDContent *parent;
    ContentType type;
};

struct CliDoc
{
    CDContent base;
    CDContent *name;
    CDContent *version;
    CDContent *author;
    CDContent *license;
    CDContent *description;
    CDContent **flags;
    CDContent **args;
    size_t nflags;
    size_t nargs;
    int defgroup;
};

struct CDList
{
    CDContent base;
    size_t n;
    CDContent **c;
};

struct CDText
{
    CDContent base;
    char *text;
};

typedef enum ParseState
{
    PS_ROOT,
    PS_VAL,
    PS_INTVAL,
    PS_FLAG,
    PS_ARG
} ParseState;

#define isws(c) (c == ' ' || c == '\t')
#define skipws(p) while(isws(*(p))) ++(p)
#define skipwsb(p) while(isws(*(p-1))) --(p)
#define err do { \
    fprintf(stderr, "parse error in line %lu", lineno); goto error; } while (0)
#define nextline (line = ((++lineno),fgets(buf, sizeof buf, doc)))

CliDoc *CliDoc_create(FILE *doc)
{
    CliDoc *self = xmalloc(sizeof *self);
    memset(self, 0, sizeof *self);
    self->base.type = CT_ROOT;

    CDContent *parent = (CDContent *)self;
    CDContent **val;
    int *intval;
    ParseState state = PS_ROOT;

    char buf[1024];
    char name[32];
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
	if (!*line) err;

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
			state = PS_FLAG;
		    }
		    else if (!strncmp(line+1, "arg ", 4))
		    {
			state = PS_ARG;
		    }
		    line = 0;
		    break;
		}
		else if (isws(*line) || *line == ':') err;
		else tmp = strchr(line, ':');
		if (!tmp || tmp == line) err;
		size_t namelen = tmp - line;
		if (namelen > 31) err;
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
		else err;
		if (val && *val) err;
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
			if (!nextline) err;
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
			    if (!tmp) err;
			    skipwsb(tmp);
			    *tmp++ = ' ';
			    *tmp = 0;
			    size_t linelen = strlen(line);
			    txt = xrealloc(txt, txtlen + linelen + 1);
			    strcpy(txt+txtlen, line);
			    txtlen += linelen;
			    if (!nextline) err;
			    skipws(line);
			}
			txt[txtlen-1] = 0;
			CDText *text = xmalloc(sizeof *text);
			text->base.parent = parent;
			text->base.type = CT_TEXT;
			text->text = txt;
			txt = 0;
			txtlen = 0;
			if (!*val) *val = (CDContent *)text;
			else if ((*val)->type == CT_LIST)
			{
			    text->base.parent = *val;
			    CDList *list = (CDList *)*val;
			    list->c = xrealloc(list->c,
				    (list->n+1) * sizeof *list->c);
			    list->c[list->n++] = (CDContent *)text;
			}
			else
			{
			    CDText *first = (CDText *)*val;
			    CDList *list = xmalloc(sizeof *list);
			    list->base.parent = parent;
			    list->base.type = CT_LIST;
			    list->n = 2;
			    list->c = xmalloc(2 * sizeof *list->c);
			    first->base.parent = (CDContent *)list;
			    text->base.parent = (CDContent *)list;
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
		    if (!tmp) err;
		    skipwsb(tmp);
		    *tmp = 0;
		    CDText *text = xmalloc(sizeof *text);
		    text->base.parent = parent;
		    text->base.type = CT_TEXT;
		    text->text = malloc(strlen(line)+1);
		    strcpy(text->text, line);
		    *val = (CDContent *)text;
		}
		goto parent;

	    case PS_INTVAL:
		if (*line == '\n') err;
		tmp = strchr(line, '\n');
		if (!tmp) err;
		skipwsb(tmp);
		if (tmp == line) err;
		errno = 0;
		tmpval = strtol(line, &endp, 10);
		if (endp != tmp) err;
		if (errno == ERANGE || tmpval > INT_MAX) err;
		*intval = tmpval;
		goto parent;

	    case PS_FLAG:
		line = 0;
		break;

	    case PS_ARG:
		line = 0;
		break;

	    parent:
		switch (parent->type)
		{
		    case CT_ROOT: state = PS_ROOT; break;
		    default: state = PS_ROOT; break;
		}
		line = 0;
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

