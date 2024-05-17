title: The 66 Suite: execl-envfile
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# execl-envfile

A mix of [s6-envdir](https://skarnet.org/software/s6/s6-envdir.html) and [importas](https://skarnet.org/software/execline/importas.html). Reads files containing variable assignments in the given *file/directory*, adds the variables to the environment and then executes a program.

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

- **-h**: prints this help.

- **-v** *verbosity*: increases/decreases the verbosity of the command.
    * *1*: only print error messages. This is the default.
    * *2*: also print warning messages.
    * *3*: also print tracing messages.
    * *4*: also print debugging messages.

- **-l**: loose; do nothing and execute *prog* directly if *src* does not contain any regular file(s) or *src* does not exist.

## File syntax

*src* is a text file or a directory containing lines of pairs with the syntax being: `key = value`
Whitespace is permitted before and after *key*, and before or after *value*.

Empty lines, or lines containing only whitespace, are ignored. Lines beginning with `#` or `;` (possibly after some whitespace) are ignored (and typically used for comments). Leading and trailing whitespace is stripped from values; but a *value* can be double-quoted, which allows for inclusion of leading and trailing whitespace.

Escaping double-quoted can be done with backslash `\`. For instance,

```
cmd_args=-g \"daemon off;\"
```

C escapes, including hexadecimal and octal sequences, are supported in quoted values. Unicode codepoint sequences are not supported.

If *value* is empty, *key* is still added to the environment, with an empty value.

If you do not want *key* to be added to the environment at all, prefix the *value* with an exclamation mark `!`. Whitespace **are not permitted** between the `!` and `value`. For instance,

```
    socket_name=!sname
```

In this case the *key* will be removed from the environment after the substitution.

Reusing the same variable or variable from the actual environment is allowed. In such case, variable name **must be** between `${}` to get it value. For intance, an environment file can be declared

```
    PATH=/usr/local/bin:${PATH}
    socket_name=sname
    socket_dir=dname
    socket=${socket_dir}/${socket_name}
```

The order of `key=value` pair declaration **do not** matter

```
    PATH=/usr/local/bin:${PATH}
    socket=${socket_dir}/${socket_name}
    socket_name=sname
    socket_dir=dname
```

### Limits

*src* can not exceed more than `100` files. Each file can not contain more than `8191` bytes or more than `50` `key=value` pairs.

## Usage example

```
    #!/usr/bin/execlineb -P
    fdmove -c 2 1
    execl-envfile %%service_admconf%%/ntpd
    foreground { mkdir -p  -m 0755 ${RUNDIR} }
    execl-cmdline -s { ntpd ${CMD_ARGS} }

```

The equivalent with s6-envdir and importas would be:

```
    #!/usr/bin/execlineb -P
    fdmove -c 2 1
    s6-envdir %%service_admconf%%
    importas -u RUNDIR RUNDIR
    importas -u CMD_ARGS CMD_ARGS
    foreground { mkdir -p  -m 0755 ${RUNDIR} }
    execl-cmdline -s { ntpd ${CMD_ARGS} }
```

where `%%service_admconf%%` contains two named files `RUNDIR` and `CMD_ARGS` written with `/run/openntpd` and `-d -s` respectively.
