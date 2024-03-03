#ifndef MKCLIDOC_MDOCWRITER_H
#define MKCLIDOC_MDOCWRITER_H

#include "decl.h"

#include <stdio.h>

C_CLASS_DECL(CliDoc);

int writeMdoc(FILE *out, const CliDoc *root)
    ATTR_NONNULL((1)) ATTR_NONNULL((2));

#endif
