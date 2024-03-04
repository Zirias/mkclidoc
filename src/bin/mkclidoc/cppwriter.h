#ifndef MKCLIDOC_CPPWRITER_H
#define MKCLIDOC_CPPWRITER_H

#include "decl.h"

#include <stdio.h>

C_CLASS_DECL(CliDoc);

int writeCpp(FILE *out, const CliDoc *root)
    ATTR_NONNULL((1)) ATTR_NONNULL((2));

#endif
