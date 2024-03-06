# mkclidoc - Simple documentation generator for CLI utilities

This is a very rudimentary tool to read a description of a command line
utility in a simple text format and produce output in C preprocessor,
troff/man or mandoc format. It only supports utilities using single-letter
flags preceded by a dash (POSIX style).

## Usage

    Usage: mkclidoc [-f <cpp|man|mdoc>[,args]] [-o outfile] [infile]

* `-f format,args`: Output format with optional format-specific args,
  defaults to `man`.
  - `cpp`: A set of C preprocessor macros to print usage and help messages
  - `man`: A manpage in classic troff/man format
  - `mdoc`: A manpage in (BSD) mandoc format
    * `mdoc,os`: Override the mandoc `.Os` value with the tool name and
      version
* `-o outfile`: Optional output file, writes to `stdout` by default
* `infile`: Optional input file, reads from `stdin` by default

## Input format

A description for `mkclidoc` is a simple text format like in this example:

    name: frob
    comment: frobnicate input
    date: 20240303
    author: John Doe <john@doe.invalid>
    version: 1.0
    license: BSD
    www: https://example.com/frob
    description:
    %%name%% converts nonsense to other, much more beautiful nonsense.
    It also applies a green tint to it.
    .

    defgroup: 1

    [flag g]
    description: stronger green
    default: mild green

    [flag h]
    group: 0
    optional: 0
    description: print a help text and exit

    [flag i intensity]
    description: how hard to try
    min: 0 (not at all)
    max: 100 (full power)
    default: 50

    [flag k kind]
    optional: 0
    description:
    %%arg%% specifies the kind of nonsense.
    | kind   | description | dangerous |
    | --     | -- | -- |
    | fun    | funny nonsense | no |
    | cringe | unsettling nonsense | no |
    | psych  | horrible nonsense | yes |
    .

    [flag m mode]
    description:
    Frobnication mode, one of:
    - [foo:]: do some foo
    - [bar:]: do some bar instead
    .
    default: foo

    [arg filename]
    description: frobnicate the file %%arg%%
    optional: 1
    default: frobnicate thin air

This example shows all currently supported fields and elements.

A field starts with `<name>:`. Most fields just contain any text, except the
following:

* `date` must use `YYYYMMDD` format
* `defgroup` and `group` are positive integers starting from `0` (and default
  to `0`). These are used to sort flags and args into groups that can be used
  together, e.g. in the above example, `-h` must be used alone and is
  mentioned first.
* `optional` is a boolean flag and must be `0` or `1`. It defaults to `0` for
  flags and `1` for args.

A text field can be given on the same line, or as a block starting on the next
line and ended by a line containing a single perion (`.`).

In a text block, there can also be tables (at least 2 columns surrounded and
separated by `|` characters) and item lists of the form `- [<key>]: <value>`.
The value for an item can exceed a single line and stops when the next item is
found or on an empty line. As a special case, the value of an item can also be
a table, which must then start on the next line after `- [<key>]:`.

A blank line somewhere inside a text block starts a new paragraph.

## Limitations

Manpage output is only rudimentarily escaped, so you might easily find edge
cases producing garbage output or even crashing.

The output formats aren't configurable.

The tool does not sort flags, you are expected to keep them in alphabetical
order (with uppercase flags first) in your input file.

