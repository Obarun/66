# 66-supervise

The per-service process supervisor. `66-supervise` keeps a single service running according to its declared policy: it runs the service's `run` script, restarts it when it dies (within a crash budget), runs its `finish` script on the way down, tracks readiness, and records the service's status. It is the native supervisor of `66` (historically inspired by s6-supervise) and is launched — one instance per service — by [66-scandir](66-scandir.html).

## Interface

```
66-supervise servicename
```

`66-supervise` is **not meant to be run directly**. [66-scandir](66-scandir.html) spawns it for every service it finds, with its current working directory set to the *scandir* and *servicename* as its only argument. The supervisor then changes into the service directory, reads the service's resolve database (`./.resolve`) and supervises it.

To act on a supervised service, use the friendly, resolve-aware commands [66 start](66-start.html), [66 stop](66-stop.html) and [66 signal](66-signal.html); the low-level, resolve-independent control primitive is [66-svctl](66-svctl.html).

## Behaviour

One `66-supervise` process supervises exactly one service:

- It runs the service's `run` script and waits for it. When the process dies it is restarted, unless a *down* request is in effect (see [Files](#files)).
- For a service that declares a readiness notification descriptor, the supervisor waits for the service to announce it is ready before reporting it *up and ready*.
- When the service is brought down, the supervisor sends it the configured *down signal*, then runs the service's `finish` script before going back to a *down* state.
- At every transition it writes the service's status to `supervise/status` so that [66 status](66-status.html) and the rest of `66` can report it.

## Configuration

Unlike a classic supervisor that reads a set of per-service files, `66-supervise` takes its configuration directly from the service's resolve database, compiled by [66 parse](66-parse.html) from the [frontend](66-frontend.html) file. The relevant frontend keys are:

- `Notify` (in `[Start]`): the file descriptor on which the service announces its readiness.
- `Timeout` (in `[Start]` / `[Stop]`): the maximum duration of the start / stop transition.
- `MaxDeath` / `MaxDeathInterval` (in `[Start]`): the crash budget (see [Crash budget](#crash-budget)).
- `DownSignal` (in `[Stop]`): the signal sent to the service to bring it down.

The only configuration kept as a runtime file is `down`: its presence means *keep the service down*. [66 start](66-start.html) and [66 stop](66-stop.html) create and remove it.

## Files

`66-supervise` manages the following entries in the service's live directory:

- `supervise/status`: a fixed-size binary status record (state, result, provenance, pid, timestamps and crash counter). It is read by [66 status](66-status.html).
- `supervise/control`: the control fifo on which the supervisor receives its commands.
- `supervise/lock`: guarantees that a single `66-supervise` process supervises a given service.
- `down`: the runtime *down* request described above.

## Crash budget

If a service dies *MaxDeath* times (or more) within *MaxDeathInterval* milliseconds, `66-supervise` stops restarting it and marks it as failed (crash limit reached). A successful [66 start](66-start.html) resets the budget. Setting `MaxDeath = 0` disables the budget entirely (infinite restart).

A service whose `run` script cannot be executed at all (for example a non-executable file) is reported as an *exec failure* with a progressive, bounded backoff; this is kept separate from the crash budget and never counts towards it.

## Control

`66-supervise` listens on `supervise/control` for single-byte commands — bring up, bring down, restart, reload, send a signal, exit, and so on. You normally never write to this fifo yourself: [66 start](66-start.html), [66 stop](66-stop.html), [66 signal](66-signal.html) and the [66-svctl](66-svctl.html) primitive do it for you.
