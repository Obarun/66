title: The 66 Suite: 66-scanctl
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# 66-scanctl

Sends a *signal* to a scandir. Safe wrapper around [s6‑svscanctl](https://skarnet.org/software/s6/s6-svscanctl.html).

## Interface

```
    66-scanctl [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -d notif ] [ -t rescan ] [ -e environment ] [ -o owner ] start|stop|reload|quit|nuke|zombies or any s6-svscanctl options.
```

This program sends a *signal* to an already running [scandir](scandir.html) at *live* where by default *live* is at `%%livedir%%` or the resulting path provided by the **‑l** option. If owner is not explicitely set with **‑o** then the user of the current process will be used instead.

## Exit codes

- *0* success
- *100* wrong usage
- *111* system call failed

## Options

- **-h** : prints this help.

- **-z** : use color.

- **-v** *verbosity* : increases/decreases the verbosity of the command.
    * *1* : only print error messages. This is the default.
    * *2* : also print warning messages.
    * *3* : also print tracing messages.
    * *4* : also print debugging messages.

- **-l** *live* : changes the supervision directory of *service* to *live*. By default this will be `%%livedir%%`. The default can also be changed at compile time by passing the `--livedir=live` option to `./configure`. An existing absolute path is expected and should be within a writable and executable filesystem - likely a RAM filesystem—see [66-scandir](66-scandir.html).

- **-d** *notif* : notify readiness on file descriptor notif. When *scandir* is ready to accept commands from [66‑scanctl](66-scanctl.html), it will write a newline to *notif*. *notif* **cannot be** lesser than `3`. By default, no notification is sent. If **-b** is set, this option have no effects.

- **-t** *rescan* : perform a scan every *rescan* milliseconds. If *rescan* is set to 0 (the default), automatic scans are never performed after the first one and [s6‑svscan](https://skarnet.org/software/s6/s6-svscan.html) will only detect new services by issuing either [66‑scanctl](66-scanctl.html) reload or [s6‑svscanctl -a](https://skarnet.org/software/s6/s6-svscanctl.html). It is **strongly** discouraged to set rescan to a positive value under `500`.

- **-e** *environment* : an absolute path. Merge the current environment variables with variables found in this directory before starting the *scandir*. Any file in environment not beginning with a dot and not containing the `=` character will be read and parsed.

- **-o** *owner* : send the *signal* to a [scandir](66-scandir.html) owned by *owner* instead of the current owner of the process. *owner* needs adecuate permissions to deal with the scandir.

## Signal

Any signal accepted by [s6‑svscanctl](https://skarnet.org/software/s6/s6-svscanctl.html) can be passed but without the dash `‑` character. As a result if you wanted to send a **‑t** signal, you need to use: `66‑scanctl t`. Further a series of commands is also accepted in the same way: `66‑scanctl st`. A few convenient keywords were added to avoid having to remember basic and useful commands:

- *reload*    = ‑an
- *interrupt* = ‑i
- *quit*      = ‑q
- *halt*      = ‑0
- *reboot*    = ‑6
- *poweroff*  = ‑7

## Usage examples

```
    66-scanctl reload
```

Updates the process supervision tree to exactly match the services listed in [scandir](66-scandir.html).
This command is strictly equal to:

```
    s6-svscanctl -an /path_to_scandir
```

[s6‑svscan](https://skarnet.org/software/s6/s6-svscan.html).
