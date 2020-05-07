title: The 66 Suite: 66-instate
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-instate

This command displays the contents of the service's *state* file. 

## Interface

```
    66-instate [ -h ] [ -z ] [ -t tree ] [ -l ] service
```

66-instate displays the contents of the service's *state* file. This file are used internally by the *66* tools to know runtime *service* information. This tool is purely a debug tool used by developers.

## Exit codes

- *0* success
- *100* wrong usage
- *111* system call failed

## Options

- **-h** : prints this help;

- **-z** : use color.

- **-t** *tree* : only searches the service at the specified *tree*, when the same service may be enabled in more trees.

- **-l** : displays the contents of the associated logger's *state* file if any. 
