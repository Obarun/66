# reload

This command reloads one or more services.

## Interface

```
reload [ -h ] [ -P ] service...
```

This command sends a `SIGHUP` signal to *service*. Many daemons react to a `SIGHUP` signal to re-read their configuration file when it has changed. This command expects to find an already running *service*: a named service that is not supervised, or supervised but not running, is refused with `100`, and nothing is reloaded. When a dependency brought back up first fails, what waits for it is not reloaded, the other services are, and the command exits with `111`.

The *service* and the services that depend on it are reloaded, never the ones it depends on. A dependent that was never started, or that was stopped by `66 stop -P`, is left down. A dependency left down by `66 stop -P` or `66 free -P` and needed by what runs is brought back up first, except under **-P**. A oneshot that is `done` runs nothing again.

A reload sends no signal event: a [reactor](66-event.html) listening for `SIGHUP` does not fire on it, and a reactor armed above the reloaded service stays armed.

The `66 -t` command option has no effect. `66` will detect automatically the associated *tree* of the service along its required-by dependencies.

In case of `module` service type, all services within the `module` are reloaded. A module one member of which was freed alone is refused with `100`.

Multiple *services* can be reloaded by separating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies).

**Note**: A logger named on the command line is reloaded with its service. If you want to reload a logger alone, use the `-P` option.

## Options

- **-h, --help**: prints this help.

- **-P, --no-propagate**: Do not handle service dependencies. In such cases, the *reload* command will not attempt to reload the services that are dependent on the service, regardless of their current state.

## Usage examples

Reloads the `foo` service

```
66 reload foo
```

Reloads the `foo-log` service

```
66 reload -P foo-log
```

## Modules services

Services within a `module` can also be managed independently. If you need to reload a particular service inside the `module`, specify the name of the `module` service followed by a colon `:` and the name of the service within the `module`

```
66 reload foo@foobar:foobaz
```

where `foo@foobar` is the name of the `module` service and `foobaz` the name of the service inside the `module` service.

You also can use the `-P` option to avoid handling the dependencies of the service inside the `module` service

```
66 reload -P foo@foobar:foobaz
```
