# internal

Starts, stops and restarts the internal services of a [scandir](66-scandir.html).

## Interface

```
internal [ -h ] [ -o owner ] start|stop|restart fdholderd|oneshotd|eventd
```

The internal services `fdholderd`, `oneshotd` and `eventd` belong to every [scandir](66-scandir.html#internal-services): `66-scandir` starts them before any other service at its first scan. This command acts on one of them.

A stop lasts until the [scandir](66-scandir.html) is started again, which brings every internal service back up.

This command expects an already running [scandir](66-scandir.html) for the relevant *owner*. It waits for the internal service to reach its state for the time given by the `-T` option of [66](66.html), without limit by default.

## Options

- **-h, --help**: print this help.
- **-o, --owner** *owner*: operate on the internal services of the given *owner*'s [scandir](66-scandir.html). Only the root user can use this option.

## Subcommands

- **start**: start the internal service.
- **stop**: stop the internal service.
- **restart**: restart the internal service.

## Usage examples

Restart the event daemon of the current owner's *scandir*

```
66 internal restart eventd
```

Stop the oneshot daemon of `owner`'s *scandir*

```
66 internal -o owner stop oneshotd
```
