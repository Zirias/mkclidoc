#include "clidoc.h"
#include "mdocwriter.h"

int main(void)
{
    CliDoc *doc = CliDoc_create(stdin);
    if (!doc) return 1;
    writeMdoc(stdout, doc);
    CliDoc_destroy(doc);
}

