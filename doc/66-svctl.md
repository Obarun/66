# 66-svctl

Low-level control primitive: writes raw command bytes directly to a *service*'s `supervise/control` fifo.

## Interface

```
66-svctl [ -h ] [ -w uUdDrR ] [ -T milliseconds ] [ -abqHkti12pcylsrodDuUxOQ ] [ --stop-group | --cont-group | --kill-group ] servicedir...
```

This program sends one or more commands to the `66-supervise` process that monitors a *service*. It operates **directly on a service directory**: it reads nothing — no resolve database, no service name resolution, no state — it just opens `servicedir/supervise/control` and writes the requested bytes.

This is the reason it exists: it is the recovery path when the resolve database is unusable (a corrupted resolve makes `66 stop foo` unable to map the name, but the control fifo still answers), and the primitive the system scripts rely on. It is installed in the libexec directory and is **not meant for everyday use** — the friendly, resolve-aware path is [66 signal](66-signal.html), which also handles dependencies and accepts service names.

Several *servicedir* can be given, separated by a space. Commands accumulate in the order they appear on the command line (clustering works: `-dx` writes `d` then `x`). If a command cannot be sent to a *servicedir*, an error is reported and the remaining directories are still processed; the exit code reflects the last failure.

Unless `-w` is given, `66-svctl` writes the commands and exits immediately without waiting for the *service* to reach any particular state.

## Options

- **-h, --help**: print this help.
- **-w, --wait** *uUdDrR*: do not exit until the *service* reaches the wanted state. The accepted values are the same as [66 signal](66-signal.html): `u` up, `U` up and ready (notified), `d` down, `D` down and ready to be brought up, `r` (re)started, `R` (re)started and ready. The wait is performed through the native event mechanism on `servicedir/event` and stays resolve-independent. If the *service* has no `notification-fd`, a readiness wait (`U`/`D`/`R`) is downgraded to its non-ready equivalent (`u`/`d`/`r`).
- **-T, --timeout** *milliseconds*: with `-w`, fail after this delay if the wanted state has not been reached. `0` (the default) waits forever.
- **-s, --signal** *signal*: send a signal to the supervised process by signal name or number, restricted to the user-available signals listed below.
- **-a, --alarm**: send a SIGALRM signal.
- **-b, --abort**: send a SIGABRT signal.
- **-q, --quit**: send a SIGQUIT signal.
- **-H, --hangup**: send a SIGHUP signal.
- **-k, --kill**: send a SIGKILL signal.
- **-t, --term**: send a SIGTERM signal.
- **-i, --interrupt**: send a SIGINT signal.
- **-1, --usr1**: send a SIGUSR1 signal.
- **-2, --usr2**: send a SIGUSR2 signal.
- **-p, --stop**: send a SIGSTOP signal.
- **-c, --cont**: send a SIGCONT signal.
- **-y, --winch**: send a SIGWINCH signal.
- **--stop-group**: send a SIGSTOP signal to the whole process group of the supervised process.
- **--cont-group**: send a SIGCONT signal to the whole process group of the supervised process.
- **--kill-group**: send a SIGKILL signal to the whole process group of the supervised process.
- **-r, --restart**: if the *service* is up, restart it by sending it a signal (default SIGTERM).
- **-l, --reload**: if the *service* is up, reload it by sending it a signal (default SIGHUP).
- **-o, --once**: once. Equivalent to `-uO`.
- **-d, --down**: bring the *service* down (SIGTERM then SIGCONT) and do not restart it.
- **-D, --down-keep**: bring the *service* down and create a down file so it is not brought up automatically.
- **-u, --up**: bring the *service* up.
- **-U, --up-restart**: bring the *service* up and remove any down file so it can be restarted automatically.
- **-x, --exit**: bring the *service* down and tell its supervisor to exit once it is down.
- **-O, --once-at-most**: mark the *service* to run once at most (do not restart it when it dies).
- **-Q, --once-at-most-down**: once at most, and bring the *service* down.

## Usage examples

Bring the *service* whose directory is `/run/66/scandir/0/foo` down

```
66-svctl -d -- /run/66/scandir/0/foo
```

Bring it up and block until it has notified readiness, giving up after 5 seconds

```
66-svctl -u -w U -T 5000 -- /run/66/scandir/0/foo
```

Bring it down and let its supervisor exit

```
66-svctl -dx -- /run/66/scandir/0/foo
```
