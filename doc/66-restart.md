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

This command handles [interdependencies](66.html#handling-dependencies). Its two halves do not go the same way. Bringing the selection down is propagated to the services that depend on it, bringing it back up is propagated to the services it depends on.

## Options

- **-h, --help**: prints this help.

- **-P, --no-propagate**: Do not handle service dependencies. In such cases, the *restart* command deals only with the services named on the command line. It neither brings down the services that depend on them, nor brings up the services they depend on, regardless of their current state.

## Usage examples

Restarts the `foo` service

```
66 restart foo
```

Restarts the `foo` service without handling its required-by dependencies

```
66 restart -P foo
```