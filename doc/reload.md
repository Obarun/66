title: The 66 Suite: reload
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# reload

This command reloads one or more services.

## Interface

```
reload [ -h ] [ -P ] service...
```

This command send a `SIGHUP` signal to *service*. Many daemon reacts of `SIGHUP` signal to re-read its configuration file that has been changed. This command expects to find an already running *service*.

The `66 -t` command option have no effect. `66` will detect automatically the associated *tree* of the service along its required-by dependencies.

In case of `module` service type, all services within the `module` are reloaded.

Multiple *services* can be reloaded by separating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies).

**Note**: If you want to reload a logger, use the `-P` option.

## Options

- **-h**: prints this help.

- **-P**: Do not handle service dependencies. In such cases, the *reload* command will not attempt to reload the services that are dependent on the service, regardless of their current state.

## Usage examples

Reloads the `foo` service

```
66 reload foo
```

Reloads the `foo-log` service

```
66 reload -P foo-log
```

