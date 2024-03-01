#ifndef MKCLIDOC_CLIDOC_H
#define MKCLIDOC_CLIDOC_H

#include "decl.h"

#include <stdio.h>

C_CLASS_DECL(CliDoc);

typedef enum ContentType
{
    CT_ROOT,
    CT_ARG,
    CT_FLAG,
    CT_LIST,
    CT_TEXT
} ContentType;

CliDoc *CliDoc_create(FILE *doc);

void CliDoc_destroy(CliDoc *self);

#endif
