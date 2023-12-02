title: The 66 Suite: 66-hpr
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# 66-hpr

Triggers the software shutdown procedure. Performs an immediate hardware shutdown with the **‑f** option. It is normally invoked by `66 halt`, `66 poweroff` or `66 reboot` command. This program is a modified copy of [s6-linux-init-hpr](https://skarnet.org/software/s6-linux-init/s6-linux-init-hpr.html).

This program is primarily used internally by `66`. User may prefer to use `66 halt`, `66 poweroff` or `66 reboot` instead.

## Interface

```
66-hpr [ -H ] [ -l live ] [ -b banner ] [ -f ] [ -h | -p | -r ] [ -n ] [ -d | -w ] [ -W ]
```

- If the **-f** option is passed the system is stopped or rebooted immediately without properly cleaning up.

- Else the machine's shutdown procedure is started.

- The command exits 0; the shutdown procedure happens asynchronously.

This is the traditional sysvinit interface for the `halt`, `poweroff` and `reboot` programs. *66‑hpr* must always be called with either **‑h**, **‑p** or **‑r**.

## Options

- **-H**: prints this help.

- **-l** *live*: changes the supervision directory of *service* to *live*. By default this will be `%%livedir%%`. The default can also be changed at compile time by passing the `--livedir=live` option to `./configure`. An existing absolute path is expected and should be within a writable and executable filesystem - likely a RAM filesystem—see [66-scandir](66-scandir.html).

- **-b** *banner*: Text to display before executing the shutdown process. Defaults to:

```
*** WARNING ***
The system is going down NOW!
```

- **-f**: force. The command will not trigger a clean shutdown procedure; it will only sync the filesystems then tell the kernel to immediately `halt`, `poweroff` and `reboot`. This should be the last step in the lifetime cycle of the machine.

- **-h**: halt. The system will be shut down but the power will remain connected.

- **-p**: poweroff. Like halt but the power will also be turned off.

- **-r**: reboot. The system will initialize a warm boot without disconnecting power.

- **-n**: Do not call [sync()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/sync.html) before the hardware shutdown. The default is to sync, just in case. This option is only relevant when combined with **-f**; without it, it has no effect.

- **-d**: Do not write a wtmp shutdown entry—see [utmp,wtmp and btmp](https://en.wikipedia.org/wiki/Utmp).

- **-w**: Only write a wtmp shutdown entry; does not actually shut down the system.

- **-W**: Do not send a *wall* message to users before shutting down the system. Some other implementations of the `halt`, `poweroff` and `reboot` commands use the `‑‑no‑wall` long option to achieve this.

## Notes

`halt`, `poweroff` and `reboot` scripts that call *66‑hpr* with the relevant option ***should be copied or symlinked*** by the administrator into the binary directory of your system.
