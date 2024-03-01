#ifndef MKCLIDOC_CLIDOC_H
#define MKCLIDOC_CLIDOC_H

#include "decl.h"

#include <stdio.h>

C_CLASS_DECL(CliDoc);
C_CLASS_DECL(CDContent);
C_CLASS_DECL(CDList);
C_CLASS_DECL(CDText);
C_CLASS_DECL(CDArg);
C_CLASS_DECL(CDFlag);

typedef enum ContentType
{
    CT_ROOT,
    CT_LIST,
    CT_TEXT,
    CT_ARG,
    CT_FLAG
} ContentType;

CliDoc *CliDoc_create(FILE *doc);

void CliDoc_destroy(CliDoc *self);

#endif
