#ifndef MKCLIDOC_MANWRITER_H
#define MKCLIDOC_MANWRITER_H

#include "decl.h"

#include <stdio.h>

C_CLASS_DECL(CliDoc);

int writeHtml(FILE *out, const CliDoc *root, const char *args)
    ATTR_NONNULL((1)) ATTR_NONNULL((2));
int writeMan(FILE *out, const CliDoc *root, const char *args)
    ATTR_NONNULL((1)) ATTR_NONNULL((2));
int writeMdoc(FILE *out, const CliDoc *root, const char *args)
    ATTR_NONNULL((1)) ATTR_NONNULL((2));

#endif
