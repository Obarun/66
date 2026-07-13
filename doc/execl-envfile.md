# execl-envfile

The environment-file tool of the `66` suite, in the spirit of execline's [importas](https://skarnet.org/software/execline/importas.html). Reads files containing variable assignments in the given *file/directory*, adds the variables to the environment and then executes a program.

## Interface

```
execl-envfile [ -h ] [ -v verbosity ] [ -l ] src prog
```
This program expects to find a regular file or a directory in *src* containing one or multiple `key=value` pair(s). It will parse that file, import the `key=value` and then exec the given *prog* with the modified environment. In case of directory for each file found it apply the same process. *src* can be an absolute or a relative path.

- It opens and reads a file.

- It parses that file.

- It imports the found `key=value` pair(s).

- It substitutes each corresponding *key* with value from that file.

- It unexports the variable(s) if requested.

- It execs *prog* with the modified environment.

## Options

- **-h, --help**: prints this help.

- **-v, --verbosity** *verbosity*: increases/decreases the verbosity of the command.
    * *1*: only print error messages. This is the default.
    * *2*: also print warning messages.
    * *3*: also print tracing messages.
    * *4*: also print debugging messages.

- **-l, --loose**: loose; do nothing and execute *prog* directly if *src* does not contain any regular file(s) or *src* does not exist.

## File syntax

*src* is a text file or a directory containing files with lines of pairs with the syntax being: `key = value`. If *src* is a directory, the directory is parsed by ascending alphabetical order. If there are duplicate `key=value` pairs, the pair found in the last file takes precedence.

Whitespace is permitted before and after *key*, and before or after *value*.

Empty lines, or lines containing only whitespace, are ignored. Lines beginning with `#` (possibly after some whitespace) are ignored (and typically used for comments). Leading and trailing whitespace is stripped from values; but a *value* can be double-quoted, which allows for inclusion of leading and trailing whitespace.

Escaping double-quoted can be done with backslash `\`. For instance,

```
cmd_args=-g \"daemon off;\"
```

C escapes, including hexadecimal and octal sequences, are supported in quoted values. Unicode codepoint sequences are not supported.

A *value* that decodes to a newline (`\n`, or its octal `\012` / hexadecimal `\x0a` form) or a NUL (`\0`, `\000`, `\x00`) is **rejected** as a syntax error. The environment is held internally as a NUL-separated list of entries and re-emitted as newline-separated text, so a value containing either byte could not survive that round-trip; the parse fails loudly instead of silently corrupting the value. (This diverges from execline's `envfile`, which permits a newline in a value.)

If *value* is empty, *key* is still added to the environment, with an empty value.

If you do not want *key* to be added to the environment at all, prefix the *value* with an exclamation mark `!`. Whitespace **are not permitted** between the `!` and `value`. For instance,

```
    socket_name=!sname
```

In this case the *key* will be removed from the environment after the substitution.

Reusing the same variable or variable from the actual environment is allowed. In such case, variable name **must be** between `${}` to get its value. For instance, an environment file can be declared

```
    PATH=/usr/local/bin:${PATH}
    socket_name=sname
    socket_dir=dname
    socket=${socket_dir}/${socket_name}
```

The order of `key=value` pair declaration **does not** matter

```
    PATH=/usr/local/bin:${PATH}
    socket=${socket_dir}/${socket_name}
    socket_name=sname
    socket_dir=dname
```

A variable calling itself is **only** allowed if the `key` name can be found at the environment of the current process. If the key of the `key=value` cannot be found it leaves the pair as is. For instance,

```
    PATH=/usr/local/bin:${PATH}
```

will only work if `PATH` is already defined in the current environment. If not the result will literally be `PATH=/usr/local/bin:${PATH}`.

### Limits

*src* can not exceed more than `20` files. Each file can not contain more than `8191` bytes or more than `50` `key=value` pairs.

## Usage example

```
    #!/usr/bin/execlineb -P
    fdmove -c 2 1
    execl-envfile %%service_admconf%%/ntpd
    foreground { mkdir -p  -m 0755 ${RUNDIR} }
    execl-cmdline -s { ntpd ${CMD_ARGS} }

```

where `%%service_admconf%%/ntpd` is a file containing the two `key=value` pairs `RUNDIR=/run/openntpd` and `CMD_ARGS=-d -s`.
