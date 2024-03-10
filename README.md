# mkclidoc - Simple documentation generator for CLI utilities

This is a very rudimentary tool to read a description of a command line
utility in a simple text format and produce output in C preprocessor, shell
script, troff/man, mandoc or HTML format. It only supports utilities using
single-letter flags preceded by a dash (POSIX style).

## Usage

    Usage: mkclidoc [-f <cpp|html|man|mdoc|sh>[,args[:args...]]]
            [-o outfile] [infile]

* `-f format,args`: Output format with optional format-specific args,
  defaults to `man`.
  - `cpp`: A set of C preprocessor macros to print usage and help messages
  - `html`: A manpage in HTML format, using an embedded CSS style by default
    * `html,style=file`: Embed the contents of `file` as the literal `<style>`
      in the output
    * `html,styleuri=uri`: Link to `uri` for CSS styling
  - `man`: A manpage in classic troff/man format
  - `mdoc`: A manpage in (BSD) mdoc format
    * `mdoc,os`: Override the mdoc `.Os` value with the tool name and version
  - `sh`: A shell script snippet defining usage() and help() functions
    * `mdoc,t=file[:sub]`: Use `file` as a template, replacing `sub` with the
      generated functions. `sub` defaults to `%%CLIDOC%%`
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
    manrefs: foobar frobrc.5
    defgroup: 1

    [flag G]
    description: lighter green
    default: mild green

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
    - [bar:]: do some bar instead, using `foobar`
    .
    default: foo

    [arg filename]
    description: frobnicate the file %%arg%%
    optional: 1
    default: frobnicate thin air

    [file /usr/local/etc/frobrc]
    description: The main configuration file for %%name%%

    [var GREEN]
    description: Overrides the default green generator

    [var GREENOPTS]
    description: Extra arguments for the green generator

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
line and ended by a line containing a single period (`.`).

In a text block, there can also be tables (at least 2 columns surrounded and
separated by `|` characters) and item lists of the form `- [<key>]: <value>`.
The value for an item can exceed a single line and stops when the next item is
found or on an empty line. As a special case, the value of an item can also be
a table, which must then start on the next line after `- [<key>]:`.

A blank line somewhere inside a text block starts a new paragraph.

Text can contain elements treated specially when outputting in a manpage (man,
mdoc or html) format:

* `<link>`: If `link` contains `://`, it is rendered as an URL, otherwise, if
  it contains `@`, it is rendered as an email address.
* `` `ref` ``: If `ref` is a name mentioned in `manrefs` (without the section),
  it is rendered as a cross-reference.

### Rendered example
Here's what the example above looks like rendered as help text (`cpp` and
`sh`):

    Usage: frob -h
           frob [-Gg] -k kind [-i intensity] [-m mode]
                [filename]

        -G            lighter green
                      default: mild green
        -g            stronger green
                      default: mild green
        -h            print a help text and exit
        -i intensity  how hard to try
                      min: 0 (not at all), max: 100 (full power), default: 50
        -k kind       kind specifies the kind of nonsense.
                      kind    description          dangerous
                      --      --                   --
                      fun     funny nonsense       no
                      cringe  unsettling nonsense  no
                      psych   horrible nonsense    yes
        -m mode       Frobnication mode, one of:
                      foo:  do some foo
                      bar:  do some bar instead, using `foobar`
                      default: foo
        filename      frobnicate the file filename
                      default: frobnicate thin air

... and as a manpage (`html`, `man` and `mdoc`, here html with default style
in desktop width and "dark mode"):

![FROB(1)](.github/assets/example.svg?raw=true)

## Limitations

Manpage output is only rudimentarily escaped, so you might easily find edge
cases producing garbage output or even crashing.

The tool does not sort flags, you are expected to keep them in alphabetical
order (with uppercase flags first) in your input file.

