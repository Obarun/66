# reboot

This command triggers the reboot procedure.

## Interface

```
reboot [ -h ] [ -a ] [ -f|F ] [ -m message ] [ -t time ] [ -W ] when
```

This command triggers the reboot procedure immediately if *when* is omitted.

The *when* provided **must be** on these formats:

- *now*: triggers the reboot sequence immediately. This is the default
- *mins* or *+mins* : relative time; triggers the reboot sequence after *mins* minutes.
- *hh:mm* : absolute time; triggers the reboot sequence when the time *hh:mm* occurs. If that time has passed for the current day, it will wait for the next day. *hh* can have 1 or 2 digits; *mm* **must have** 2 digits.

## Options

- **-h, --help**: print this help.
- **-a, --access**: use access control. The reboot sequence will only be launched if one of the users listed in `/etc/66/shutdown.allow` is currently logged in (as tracked by utmp). `/etc/66/shutdown.allow` is a text file which accepts one user per line. Lines starting with # are commented out.
- **-f, --force**: do not trigger a clean shutdown procedure; it will just sync the filesystems then tell the kernel to immediately reboot. This should be the last step in the lifetime of the machine.
- **-F, --force-nosync**: same as `-f` but do not sync the filesystems.
- **-m, --message** *message*: replace the default message by message. message is broadcast to all logged in users (as tracked by utmp).
- **-t, --timeout** *time*: have a grace time period of *time* seconds between the `SIGTERM` and the `SIGKILL` at the end of the reboot sequence when it is time to kill all processes (allows processes to receive `SIGTERM` to exit cleanly). The default is `3` seconds.
- **-W, --no-wall**: do not send a wall message to users.

## Usage examples

Reboots the system.

```
66 reboot
```

Reboots a broken system

```
66 reboot -f
```

Reboots the system after 10 minutes

```
66 reboot 10
```

Sends a "system will be rebooted in 10 minutes" message to connected accounts and reboots the system after 10 minutes

```
66 reboot -m "system will be rebooted in 10 minutes" 10
```
