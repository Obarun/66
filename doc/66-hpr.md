# 66-hpr

Triggers the software shutdown procedure. Performs an immediate hardware shutdown with the **‑f** option. It is normally invoked by `66 halt`, `66 poweroff`, `66 reboot`, `66 suspend` or `66 hibernate` command. This program is a modified copy of [s6-linux-init-hpr](https://skarnet.org/software/s6-linux-init/s6-linux-init-hpr.html).

This program is primarily used internally by `66`. User may prefer to use `66 halt`, `66 poweroff`, `66 reboot`, `66 suspend` or `66 hibernate` instead.

## Interface

```
66-hpr [ -H ] [ -l live ] [ -b banner ] [ -f ] [ -h | -p | -r | -s | -i ] [ -n ] [ -d | -w ] [ -W ]
```

- If the **-s** or **-i** option is passed the system is suspended or hibernated: the kernel power state is written synchronously and the command returns once the machine wakes up. No service is stopped.

- Else if the **-f** option is passed the system is stopped or rebooted immediately without properly cleaning up.

- Else the machine's shutdown procedure is started.

- For **-h**, **-p** and **-r**, the command exits 0 and the shutdown procedure happens asynchronously.

This is the traditional sysvinit interface for the `halt`, `poweroff` and `reboot` programs. *66‑hpr* must always be called with either **‑h**, **‑p**, **‑r**, **‑s** or **‑i**.

## Options

- **-H**: prints this help.

- **-l** *live*: changes the supervision directory of *service* to *live*. By default this will be `%%livedir%%`. The default can also be changed at compile time by passing the `--livedir=live` option to `./configure`. An existing absolute path is expected and should be within a writable and executable filesystem - likely a RAM filesystem—see [66 scandir](66-scandir.html).

- **-b** *banner*: Text to display before executing the shutdown process. Defaults to:

```
*** WARNING ***
The system is going down NOW!
```

- **-f**: force. The command will not trigger a clean shutdown procedure; it will only sync the filesystems then tell the kernel to immediately `halt`, `poweroff` and `reboot`. This should be the last step in the lifetime cycle of the machine.

- **-h**: halt. The system will be shut down but the power will remain connected.

- **-p**: poweroff. Like halt but the power will also be turned off.

- **-r**: reboot. The system will initialize a warm boot without disconnecting power.

- **-s**: suspend. The system is suspended to RAM by writing `mem` to `/sys/power/state`. The call blocks until the machine wakes up; no service is stopped.

- **-i**: hibernate. The system is hibernated to disk by writing `disk` to `/sys/power/state`. This requires a properly configured swap device. The call blocks until the machine wakes up; no service is stopped.

- **-n**: Do not call [sync()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/sync.html) before the hardware shutdown. The default is to sync, just in case. This option is relevant when combined with **-f**, **-s** or **-i**; without one of them, it has no effect.

- **-d**: Do not write a wtmp shutdown entry—see [utmp,wtmp and btmp](https://en.wikipedia.org/wiki/Utmp).

- **-w**: Only write a wtmp shutdown entry; does not actually shut down the system.

- **-W**: Do not send a *wall* message to users before shutting down the system. Some other implementations of the `halt`, `poweroff` and `reboot` commands use the `‑‑no‑wall` long option to achieve this.
