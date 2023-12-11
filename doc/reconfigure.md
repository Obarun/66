title: The 66 Suite: reconfigure
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# reconfigure

This command bring down, unsupervise, parse again and bring up service.

## Interface

```
reconfigure [ -h ] [ -P ] service
```

This command is a convenient way to execute the [stop](stop.html), [free](free.html), [parse](parse.html), and [start](start.html) commands simultaneously. It's used when you modify a *service*'s frontend file and want to apply the modifications.

If the *service* is running, it is stopped and then unsupervised. Afterward, the *service* is parsed again and restarted. If the *service* isn't running, only the parse process is executed.

For `module` *service* types, the same process is applied to all services within the *module*. If the module is part of a tree associated with the boot [group](tree.html#groups-behavior), only the parse process is executed to avoid interrupting the boot sequence. The changes will be applied during the next [reboot](reboot.html).

Multiple *services* can be disabled by seperating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies) for the [stop](stop.html), [free](free.html) and [start](start.html) process.

## Options

- **-h**: prints this help.

- **-P**: Do not handle service dependencies. In such cases, the *reconfigure* command will not attempt to stop/start the services that are dependent on the service, regardless of their current state and only apply the parse process.

## Usage examples

Reconfigures the `foo` service

```
66 reconfigure foo
```

Reconfigures the `foo@bar` module service

```
66 reconfigure foo@bar
```