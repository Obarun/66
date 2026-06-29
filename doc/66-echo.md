# 66-echo

A minimal `echo` utility bundled with the `66` suite, so that `66` does not need an external portable-utils package at compilation time.

`66-echo` writes its arguments to standard output, separated by a space character and followed by a newline.

## Interface

```
66-echo [ -h ] [ -n ] [ -s separator ] args...
```

## Options

- **-h, --help**: prints this help.

- **-n, --no-newline**: do not output the trailing newline.

- **-s, --separator** *separator*: use *separator* as the character placed between arguments instead of a space.
