# reconfigure

This command brings down, unsupervises, parses again and brings up the service.

## Interface

```
reconfigure [ -h ] [ -P ] [ -c ] service...
```

This command is a convenient way to execute the [stop](66-stop.html), [free](66-free.html), [parse](66-parse.html), and [start](66-start.html) commands simultaneously. It's used when you modify a *service*'s frontend file and want to apply the modifications.

If the *service* is running, it is stopped and then unsupervised. Afterward, the *service* is parsed again and restarted. If the *service* isn't running, only the parse process is executed.

The *service* is put back in the state the command found it in: running or not, supervised or not, enabled or not, in its tree. A reactor found armed is armed again. A service never parsed is simply parsed.

Every named frontend is checked before anything is stopped: a frontend the [parse](66-parse.html) refuses stops nothing and writes nothing. The check runs no script of the service, except the `configure` script of a [module](66-module-creation.html#step-4-the-configure-script), which is part of the parse of the module and writes in a temporary copy only; a `configure` script that fails makes the command exit with `111`, nothing written. The frontends are checked once more just before the write: when another command wrote meanwhile and the check now refuses, nothing is written, the services the command took out of the supervision are put back in the state it found them in, and the command exits with `100`.

If a service fails to stop, the command exits with `111` and writes nothing: what it took out of the supervision is put back in the state it found it in.

For `module` *service* types, the same process is applied to all services within the *module*. A service inside a module cannot be reconfigured alone: name the module instead. If the module is part of a tree associated with the boot [group](66-tree.html#groups-behavior), only the parse process is executed to avoid interrupting the boot sequence. The changes will be applied during the next [reboot](66-reboot.html).

A logger is reconfigured with its service: `66 reconfigure foo-log` is refused, `66 reconfigure foo` handles both.

Multiple *services* can be reconfigured by separating their names with a space. Only the named services are parsed again, not the services that depend on them.

This command handles [interdependencies](66.html#handling-dependencies) for the [stop](66-stop.html), [free](66-free.html) and [start](66-start.html) process. The stop and free part is propagated to the services that depend on the selection, the start part to the services it depends on.

## Options

- **-h, --help**: prints this help.

- **-P, --no-propagate**: Do not handle service dependencies. In such cases, the *reconfigure* command will not attempt to stop/start the services that depend on the service nor the ones it depends on, regardless of their current state. The *services* named on the command line are still stopped, unsupervised, parsed again and brought back up. Their logger is stopped, parsed again and brought back with them. A dependency never parsed is not parsed.

- **-c, --cleanup**: remove what the named services left on disk before parsing them again, as [remove](66-remove.html) does, their log directory and their configuration included: the cleanup starts again from nothing. Their neighbours are untouched. The services are then parsed again and put back in their state. This is the repair of a service whose resolve file or one of its addons does not read: the service finds its tree again, and its activation when it can be read. When its resolve file does not read, its activation cannot be known and the service is left disabled, said at verbosity 2: enable it again with [66 enable](66-enable.html) if it was. A service whose frontend is gone is refused, remove it with [66 remove](66-remove.html).

## Usage examples

Reconfigures the `foo` service

```
66 reconfigure foo
```

Reconfigures the `foo@bar` module service

```
66 reconfigure foo@bar
```

Repairs the `foo` service whose resolve file does not read

```
66 reconfigure -c foo
```
