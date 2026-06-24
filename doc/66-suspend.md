# suspend

This command suspends the system to RAM.

## Interface

```
suspend [ -h ]
```

This command suspends the machine immediately by writing `mem` to `/sys/power/state`. The call blocks until the system is woken up, then returns.

Unlike `66 halt`, `66 poweroff` and `66 reboot`, suspending does **not** stop any service: the whole system state is kept in RAM and restored on wake-up. There is no clean shutdown procedure, no grace time and no scheduled *when* argument.

This command requires root privileges.

## Options

- **-h, --help**: print this help.

## Usage examples

Suspends the system to RAM.

```
66 suspend
```
