title: The 66 Suite: disable
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# disable

This command disables one or more services.

## Interface

```
disable [ -h ] [ -S ] service(s)
```

This command expects to find an already enabled *service*. The targeted service(s) can also be stopped on the fly when disabling it with the **-S** option. Generally speaking this command is the strict opposite of the [enable](enable.html) command.

The `66 -t` command option have no effect. `66` will detect automatically the associated *tree* of the service along its required-by dependencies.

In case of `module` service type, all services within the `module` are disabled.

Multiple *services* can be disabled by seperating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h**: prints this help.

- **-S**: stops the *service* on the fly directly after disabling it. If the state of the *service* is already down, this option will have no effect. This also applies to the required-by  dependencies of the service.

## Usage examples

Disable the service

```
66 disable foo
```

Disable an intanced service

```
66 disable foo@foobar
```

Disable and stop the service, also increase the default verbosity:

```
66 -v3 disable -S foo
```
