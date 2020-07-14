title: The 66 Suite: 66-scanctl
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-scanctl

Sends a *signal* to a scandir. Safe wrapper around [s6‑svscanctl](https://skarnet.org/software/s6/s6-svscanctl.html). 

## Interface

```
    66-scanctl [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -o owner ] signal
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
