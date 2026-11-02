# internal

Starts, stops, restarts and displays the state of the internal services of a [scandir](66-scandir.html).

## Interface

```
internal [ -h ] [ -o owner ] start|stop|restart|status fdholderd|oneshotd|eventd
```

The internal services `fdholderd`, `oneshotd` and `eventd` belong to every [scandir](66-scandir.html#internal-services): `66-scandir` starts them before any other service at its first scan. This command acts on one of them, or displays their state.

A stop lasts until the [scandir](66-scandir.html) is started again, which brings every internal service back up.

This command expects an already running [scandir](66-scandir.html) for the relevant *owner*, and exits with `100` otherwise. The **start**, **stop** and **restart** subcommands wait for the internal service to reach its state for the time given by the `-T` option of [66](66.html), without limit by default.

## Options

- **-h, --help**: print this help.
- **-o, --owner** *owner*: operate on the internal services of the given *owner*'s [scandir](66-scandir.html). Only the root user can use this option.

## Subcommands

- **start**: start the internal service.
- **stop**: stop the internal service.
- **restart**: restart the internal service.
- **status**: display the state of the internal service, or of the three in the order `fdholderd`, `oneshotd`, `eventd` when none is given. Each line holds the name of the internal service followed by the state line of [66 status](66-status.html): the state, the pid when it runs, the time since the state changed and who changed it. The state reads `None` when no supervisor of the internal service runs. It sends nothing and waits for nothing: the `-T` option of [66](66.html) has no effect. It exits `0` whatever the state of the internal services.

## Usage examples

Restart the event daemon of the current owner's *scandir*

```
66 internal restart eventd
```

Stop the oneshot daemon of `owner`'s *scandir*

```
66 internal -o owner stop oneshotd
```

Display the state of the internal services of the current owner's *scandir*

```
66 internal status
fdholderd : up (pid 16) since 2min 3s by self
oneshotd  : down since 5s by user
eventd    : up (pid 45) since 2min 3s by self
```
