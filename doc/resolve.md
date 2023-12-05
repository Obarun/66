title: The 66 Suite: resolve
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# resolve

This command displays the contents of the service's *resolve* file.

## Interface

```
resolve [ -h ] service
```

This command displays the contents of the service's *resolve* file. This file are used internally by the `66` program to know *service* information. This command is purely a debug command used by system administrators or developers.

[Resolve](deeper.html#Resolve-files) files are at the core of `66` for service information. They are used internally by `66` to build the dependency graph, ascertain file locations, the parse process result of a service, and other critical aspects of a service.

## Options

- **-h** : prints this help.

## Usage example

Displays the *resolve* file of service `foo`
```
66 resolve foo
```