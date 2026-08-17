# reconfigure

This command brings down, unsupervises, parses again and brings up the service.

## Interface

```
reconfigure [ -h ] [ -P ] service
```

This command is a convenient way to execute the [stop](66-stop.html), [free](66-free.html), [parse](66-parse.html), and [start](66-start.html) commands simultaneously. It's used when you modify a *service*'s frontend file and want to apply the modifications.

If the *service* is running, it is stopped and then unsupervised. Afterward, the *service* is parsed again and restarted. If the *service* isn't running, only the parse process is executed.

For `module` *service* types, the same process is applied to all services within the *module*. If the module is part of a tree associated with the boot [group](66-tree.html#groups-behavior), only the parse process is executed to avoid interrupting the boot sequence. The changes will be applied during the next [reboot](66-reboot.html).

Multiple *services* can be reconfigured by separating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies) for the [stop](66-stop.html), [free](66-free.html) and [start](66-start.html) process. The stop and free part is propagated to the services that depend on the selection, the start part to the services it depends on.

## Options

- **-h, --help**: prints this help.

- **-P, --no-propagate**: Do not handle service dependencies. In such cases, the *reconfigure* command will not attempt to stop/start the services that depend on the service nor the ones it depends on, regardless of their current state. The *services* named on the command line are still stopped, unsupervised, parsed again and brought back up.

## Usage examples

Reconfigures the `foo` service

```
66 reconfigure foo
```

Reconfigures the `foo@bar` module service

```
66 reconfigure foo@bar
```