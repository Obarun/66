title: The 66 Suite: 66-inresolve
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# 66-inresolve

This command displays the contents of the service's *resolve* file.

## Interface

```
    66-inresolve [ -h ] [ -z ] [ -t tree ] [ -l ] service
```

66-inresolve displays the contents of the service's *resolve* file. This file are used internally by the *66* tools to know *service* information. This tool is purely a debug tool used by developers.

## Exit codes

- *0* success
- *100* wrong usage
- *111* system call failed

## Options

- **-h** : prints this help;

- **-z** : use color.

- **-t** *tree* : only searches the service at the specified *tree*, when the same service may be enabled in more trees.

- **-l** : displays the contents of the associated logger's *resolve* file if any.
