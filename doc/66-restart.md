# restart

This command restarts one or more services.

## Interface

```
restart [ -h ] [ -P ] service...
```

This command brings down and brings up again a *service*. This command expects to find an already supervised *service*: a service that is not supervised is refused with `100`, start it first. A supervised service at rest is started.

What depends on the *service* and runs is stopped with it, then everything is brought back up with what it needs; a service that depends on it and was not running stays down. A dependency left down by `66 stop -P` or `66 free -P` is brought back up. When a stop fails, the start is sent all the same, and the command exits with `111`. When a start fails, what waits for it is not started; the reactors that do not wait for it are armed again, none is brought up.

A [reactor](66-event.html) named on the command line is brought up now, under a new process. A reactor armed above the restarted service comes back armed, it is not run. An event source stays armed.

A logger named on the command line is restarted with its service; under **-P**, the logger only.

The `66 -t` command option has no effect. `66` will detect automatically the associated *tree* of the service along its required-by dependencies.

In case of `module` service type, all services within the `module` are restarted. A module one member of which was freed alone is refused with `100`.

Multiple *services* can be restarted by separating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies). Its two halves do not go the same way. Bringing the selection down is propagated to the services that depend on it, bringing it back up is propagated to the services it depends on.

## Options

- **-h, --help**: prints this help.

- **-P, --no-propagate**: Do not handle service dependencies. In such cases, the *restart* command deals only with the services named on the command line. It neither brings down the services that depend on them, nor brings up the services they depend on, regardless of their current state. A dependency never parsed is not parsed.

## Usage examples

Restarts the `foo` service

```
66 restart foo
```

Restarts the `foo` service without handling its required-by dependencies

```
66 restart -P foo
```