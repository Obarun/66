# restart

This command restarts one or more services.

## Interface

```
restart [ -h ] [ -P ] service...
```

This command brings down and brings up again a *service*. This command expects to find an already running *service*.

The `66 -t` command option has no effect. `66` will detect automatically the associated *tree* of the service along its required-by dependencies.

In case of `module` service type, all services within the `module` are restarted.

Multiple *services* can be restarted by separating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h, --help**: prints this help.

- **-P, --no-propagate**: Do not handle service dependencies. In such cases, the *restart* command will not attempt to restart the services that are dependent on the service, regardless of their current state.

## Usage examples

Restarts the `foo` service

```
66 restart foo
```

Restarts the `foo` service without handling its required-by dependencies

```
66 restart -P foo
```