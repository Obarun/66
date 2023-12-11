title: The 66 Suite: state
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# state

This command displays the contents of the service's *state* file.

## Interface

```
state [ -h ] service
```

This command displays the contents of the service's *state* file. This file are used internally by the `66` program to know runtime *service* information. This command is purely a debug command used by system administrator or developers.

## Options

- **-h**: prints this help;

## Usage example

Display state of service `foo`

```
66 state foo
```