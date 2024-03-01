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
    CT_DICT,
    CT_TABLE,
    CT_TEXT
} ContentType;

CliDoc *CliDoc_create(FILE *doc);
ContentType CliDoc_type(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CliDoc_parent(const CliDoc *self) CMETHOD ATTR_PURE;

const CliDoc *CDRoot_name(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_version(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_author(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_license(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_description(const CliDoc *self) CMETHOD ATTR_PURE;
size_t CDRoot_nflags(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_flag(const CliDoc *self, size_t i) CMETHOD ATTR_PURE;
size_t CDRoot_nargs(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_arg(const CliDoc *self, size_t i) CMETHOD ATTR_PURE;
int CDRoot_defgroup(const CliDoc *self) CMETHOD ATTR_PURE;

void CliDoc_destroy(CliDoc *self);

#endif
