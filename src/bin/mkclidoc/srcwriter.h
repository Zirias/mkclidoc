#ifndef MKCLIDOC_SRCWRITER_H
#define MKCLIDOC_SRCWRITER_H

#include "decl.h"

#include <stdio.h>

C_CLASS_DECL(CliDoc);

int writeCpp(FILE *out, const CliDoc *root, const char *args)
    ATTR_NONNULL((1)) ATTR_NONNULL((2));
int writeSh(FILE *out, const CliDoc *root, const char *args)
    ATTR_NONNULL((1)) ATTR_NONNULL((2));

#endif
