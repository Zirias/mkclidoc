#ifndef MKCLIDOC_MANWRITER_H
#define MKCLIDOC_MANWRITER_H

#include "decl.h"

#include <stdio.h>

C_CLASS_DECL(CliDoc);

int writeMan(FILE *out, const CliDoc *root)
    ATTR_NONNULL((1)) ATTR_NONNULL((2));

#endif
