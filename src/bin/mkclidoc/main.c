#include "clidoc.h"
#include "manwriter.h"
#include "srcwriter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*writer)(FILE *out, const CliDoc *root, const char *args);

static const struct {
    const char *name;
    writer writefunc;
} writers[] = {
    { "cpp", writeCpp },
    { "man", writeMan },
    { "mdoc", writeMdoc },
    { "sh", writeSh }
};

static writer currentWriter = writeMan;
char *writerArgs = 0;
const char *infilename = 0;
const char *outfilename = 0;
static FILE *infile = 0;
static FILE *outfile = 0;

int main(int argc, char **argv)
{
    int flags = 1;
    char *name = argv[0];
    const char *arg;
    if (!name) name = "mkclidoc";

    for (++argv, --argc; argc ; ++argv, --argc)
    {
	if (**argv != '-') flags = 0;
	if (flags) switch((*argv)[1])
	{
	    case '-':
		if ((*argv)[2]) goto usage;
		flags = 0;
		break;

	    case 'f':
		if (!(*argv)[2])
		{
		    if (!argc--) goto usage;
		    arg = *++argv;
		} else arg = *argv + 2;
		currentWriter = 0;
		writerArgs = strchr(arg, ',');
		if (writerArgs)
		{
		    *writerArgs++ = 0;
		    if (!*writerArgs) writerArgs = 0;
		}
		for (unsigned i = 0; i < sizeof writers / sizeof *writers; ++i)
		{
		    if (!strcmp(arg, writers[i].name))
		    {
			currentWriter = writers[i].writefunc;
			break;
		    }
		}
		if (!currentWriter) goto usage;
		break;

	    case 'o':
		if (!(*argv)[2])
		{
		    if (!argc--) goto usage;
		    outfilename = *++argv;
		} else outfilename = *argv + 2;
		break;

	    default:
		goto usage;
	}
	else
	{
	    if (infilename) goto usage;
	    infilename = *argv;
	}
    }

    int rc = EXIT_FAILURE;
    FILE *in = stdin;
    CliDoc *doc = 0;

    if (infilename)
    {
	if (!(infile = fopen(infilename, "r"))) goto done;
	in = infile;
    }
    FILE *out = stdout;
    if (outfilename)
    {
	if (!(outfile = fopen(outfilename, "w"))) goto done;
	out = outfile;
    }

    if (!(doc = CliDoc_create(in))) goto done;
    if (currentWriter(out, doc, writerArgs) < 0) goto done;
    rc = EXIT_SUCCESS;

done:
    CliDoc_destroy(doc);
    if (infile) fclose(infile);
    if (outfile) fclose(outfile);
    return rc;

usage:
    fprintf(stderr, "Usage: %s [-f <cpp|man|mdoc|sh>[,args]]"
	    "[-o outfile] [infile]\n", name);
    return EXIT_FAILURE;
}

