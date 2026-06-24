# hibernate

This command hibernates the system to disk.

## Interface

```
hibernate [ -h ]
```

This command hibernates the machine immediately by writing `disk` to `/sys/power/state`. The call blocks until the system is woken up, then returns.

Unlike `66 halt`, `66 poweroff` and `66 reboot`, hibernating does **not** stop any service: the whole system state is saved to the swap device and restored on wake-up. There is no clean shutdown procedure, no grace time and no scheduled *when* argument.

Hibernation requires a properly configured swap device (the kernel `resume` parameter must point to it). If no resume device is available, the command fails and the system stays up.

This command requires root privileges.

## Options

- **-h**: print this help.

## Usage examples

Hibernates the system to disk.

```
66 hibernate
```
