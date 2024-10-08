title: The 66 Suite: halt
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# halt

This command triggers the halt procedure.

## Interface

```
halt [ -h ] [ -a ] [ -f|F ] [ -m message ] [ -t time ] [ -W ] when
```

This command triggers the halt procedure immediately if *when* is omitted.

The *when* provided **must be** on these formats:

- *now*: triggers the halt sequence immediately. This is the default
- *mins* or *+mins* : relative time; triggers the halt sequence after *mins* minutes.
- *hh:mm* : absolute time; triggers the halt sequence when the time *hh:mm* occurs. If that time has passed for the current day, it will wait for the next day. *hh* can have 1 or 2 digits; *mm* **must have** 2 digits.

## Options

- **-h**: print this help.
- **-a**: use access control. The halt sequence will only be launched if one of the users listed in `/etc/66/shutdown.allow` is currently logged in (as tracked by utmp). `/etc/66/shutdown.allow` is a text file which accepts one user per line. Lines starting with # are commented out.
- **-f**: do not trigger a clean shutdown procedure; it will just sync the filesystems then tell the kernel to immediately halt. This should be the last step in the lifetime of the machine.
- **-F**: same as `-f` but do not sync the filesystems.
- **-m** *message*: replace the default message by message. message is broadcast to all logged in users (as tracked by utmp).
- **-t** *time*: have a grace time period of *time* seconds between the `SIGTERM` and the `SIGKILL` at the end of the halt sequence when it is time to kill all processes (allows processes to receive `SIGTERM` to exit cleanly). The default is `3` seconds.
- **-W**: do not send a wall message to users.

## Usage examples

Halts the system.

```
66 halt
```

Halts a broken system

```
66 halt -f
```

Halts the system after 10 minutes

```
66 halt 10
```

Sends an "system will be halted in 10 minutes" to connected account and halt the system after 10 minutes

```
66 halt -m "system will be halted in 10 minutes" 10
```
