#include "clidoc.h"
#include "manwriter.h"

int main(void)
{
    CliDoc *doc = CliDoc_create(stdin);
    if (!doc) return 1;
    writeMan(stdout, doc);
    CliDoc_destroy(doc);
}

