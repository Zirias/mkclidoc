#include "clidoc.h"

int main(void)
{
    CliDoc *doc = CliDoc_create(stdin);
    CliDoc_destroy(doc);
}

