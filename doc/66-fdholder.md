# fdholder

Manages the *fdholder* daemon of a [scandir](66-scandir.html) and the file descriptors it keeps.

## Interface

```
fdholder [ -h ] [ -o owner ] start|stop|restart|store|retrieve|list [<subcommand options>]
```

The *fdholder* (`66-fdholderd`) is a daemon supervised by the [scandir](66-scandir.html). It holds open file descriptors on behalf of services so they survive a service restart — typically the read end of a logging fifo. This command controls that daemon and the descriptors it stores.

This command expects an already running [scandir](66-scandir.html) for the relevant *owner*.

## Options

- **-h, --help**: print this help.
- **-o, --owner** *owner*: operate on the *fdholder* of the given *owner*'s [scandir](66-scandir.html). Only the root user can use this option.

## Subcommands

- **start**: start the scandir's *fdholder* daemon.
- **stop**: stop the scandir's *fdholder* daemon.
- **restart**: restart the scandir's *fdholder* daemon.
- **store** *id*: store a file descriptor under the name *id*.
- **retrieve** *id* *prog...*: retrieve the descriptor stored under *id*, place it on standard input, and execute *prog*.
- **list**: list the names of the stored descriptors.

### start, stop, restart

- **-h, --help**: print this help.
- **-T, --timeout** *milliseconds*: timeout to wait for the service to reach its state.

### store *id*

- **-h, --help**: print this help.
- **-T, --timeout** *milliseconds*: timeout for the operation.
- **-d, --fd** *number*: file descriptor to store (default 0, i.e. standard input).
- **-e, --expire** *seconds*: drop the descriptor automatically after this many seconds (0 = never).

### retrieve *id* *prog...*

- **-h, --help**: print this help.
- **-T, --timeout** *milliseconds*: timeout for the operation.
- **-D, --delete**: delete the entry from the *fdholder* after retrieving it.

### list

- **-h, --help**: print this help.
- **-T, --timeout** *milliseconds*: timeout for the operation.

## Usage examples

Start the *fdholder* daemon of the current owner's *scandir*

```
66 fdholder start
```

Store standard input under the name `mylog`, keeping it forever

```
66 fdholder store mylog
```

Store file descriptor 3 under the name `mylog`, expiring after one hour

```
66 fdholder store -d 3 -e 3600 mylog
```

Retrieve `mylog` on standard input and exec a logger, deleting the entry afterwards

```
66 fdholder retrieve -D mylog 66-log -- /var/log/mylog
```

List the stored descriptors of `owner`'s *fdholder*

```
66 fdholder -o owner list
```
