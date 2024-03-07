#ifndef MKCLIDOC_CLIDOC_H
#define MKCLIDOC_CLIDOC_H

#include "decl.h"

#include <stdio.h>
#include <time.h>

C_CLASS_DECL(CliDoc);

typedef enum ContentType
{
    CT_ROOT,
    CT_ARG,
    CT_FLAG,
    CT_LIST,
    CT_DICT,
    CT_TABLE,
    CT_NAMED,
    CT_TEXT,
    CT_DATE,
    CT_MREF
} ContentType;

CliDoc *CliDoc_create(FILE *doc);
ContentType CliDoc_type(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CliDoc_parent(const CliDoc *self) CMETHOD ATTR_PURE;

const CliDoc *CDRoot_name(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_version(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_comment(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_author(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_license(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_description(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_date(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_www(const CliDoc *self) CMETHOD ATTR_PURE;
size_t CDRoot_nflags(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_flag(const CliDoc *self, size_t i) CMETHOD ATTR_PURE;
size_t CDRoot_nargs(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_arg(const CliDoc *self, size_t i) CMETHOD ATTR_PURE;
size_t CDRoot_nfiles(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_file(const CliDoc *self, size_t i) CMETHOD ATTR_PURE;
size_t CDRoot_nrefs(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDRoot_ref(const CliDoc *self, size_t i) CMETHOD ATTR_PURE;
int CDRoot_defgroup(const CliDoc *self) CMETHOD ATTR_PURE;

const CliDoc *CDArg_description(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDArg_default(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDArg_min(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDArg_max(const CliDoc *self) CMETHOD ATTR_PURE;
const char *CDArg_arg(const CliDoc *self) CMETHOD ATTR_PURE;
int CDArg_group(const CliDoc *self) CMETHOD ATTR_PURE;
int CDArg_optional(const CliDoc *self) CMETHOD ATTR_PURE;

#define CDFlag_description(self) CDArg_description(self)
#define CDFlag_default(self) CDArg_default(self)
#define CDFlag_min(self) CDArg_min(self)
#define CDFlag_max(self) CDArg_max(self)
#define CDFlag_arg(self) CDArg_arg(self)
#define CDFlag_group(self) CDArg_group(self)
#define CDFlag_optional(self) CDArg_optional(self)
char CDFlag_flag(const CliDoc *self) CMETHOD ATTR_PURE;

size_t CDList_length(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDList_entry(const CliDoc *self, size_t i) CMETHOD ATTR_PURE;

size_t CDDict_length(const CliDoc *self) CMETHOD ATTR_PURE;
const char *CDDict_key(const CliDoc *self, size_t i) CMETHOD ATTR_PURE;
const CliDoc *CDDict_val(const CliDoc *self, size_t i) CMETHOD ATTR_PURE;

size_t CDTable_width(const CliDoc *self) CMETHOD ATTR_PURE;
size_t CDTable_height(const CliDoc *self) CMETHOD ATTR_PURE;
const char *CDTable_cell(const CliDoc *self, size_t x, size_t y)
    CMETHOD ATTR_PURE;

const char *CDNamed_name(const CliDoc *self) CMETHOD ATTR_PURE;
const CliDoc *CDNamed_description(const CliDoc *self) CMETHOD ATTR_PURE;

const char *CDText_str(const CliDoc *self) CMETHOD ATTR_PURE;

time_t CDDate_date(const CliDoc *self) CMETHOD ATTR_PURE;

const char *CDMRef_name(const CliDoc *self) CMETHOD ATTR_PURE;
const char *CDMRef_section(const CliDoc *self) CMETHOD ATTR_PURE;

void CliDoc_destroy(CliDoc *self);

#endif
