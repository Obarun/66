# 66-eventd

The event daemon. `66-eventd` runs the [event system](66-event.html) at runtime: it arms and disarms event **sources** and **reactors**, watches the timers, schedules and filesystem paths they declare, and — when a trigger fires — runs the reactor's `66` command on the reactor itself. It is the event-side counterpart of the per-service supervisor [66-supervise](66-supervise.html), and like it is launched by [66-scandir](66-scandir.html); unlike it, a single `66-eventd` serves the whole scandir.

## Interface

```
66-eventd
```

`66-eventd` is **not meant to be run directly**. [66-scandir](66-scandir.html) spawns one instance for the scandir it manages. You drive it through the ordinary resolve-aware commands: [66 start](66-start.html) and [66 stop](66-stop.html) on an `event` source or on a reactor **arm** and **disarm** the corresponding rule, and [66 emit](66-emit.html) raises a `user` event. You never talk to the daemon's socket yourself.

The keys a source and a reactor declare are documented in [66-frontend](66-frontend.html#section-event); the model and the runtime rules the daemon enforces are documented in [66-event](66-event.html).

## Behaviour

One `66-eventd` process serves every event rule in the scandir:

- On an **arm** request it registers a source or a reactor. For a `timer`, `inotify` or `schedule` **source** it starts the underlying watcher (a timerfd, an inotify watch, a cron schedule) and writes the source's status. For a **reactor** it subscribes to each of its sources and immediately re-evaluates it against their current state (see [firing immediately on arm](66-event.html#firing-immediately-on-arm)).
- When a source fires, it evaluates every reactor that listens to it. It apply the [`On`/`OnAll`](66-event.html#on-versus-onall) match, the [state gating](66-event.html#when-a-reactor-actually-fires-state-gating), the [in-flight latch](66-event.html#the-in-flight-latch) and the [backstop](#anti-loop-backstop), then forks the matching `66` subcommand (`start`, `stop`, `restart`, `reload`, `reconfigure`, `free`) to act on the reactor. The subcommand walks the dependency chain like the bare command, unless the rule set [`Propagate`](66-frontend.html#propagate) to false carring the `-P` option. Actions run with an event provenance, so [66 status](66-status.html) reports *who* changed the service as `event`.
- A reactor may also **raise** a named event (`Emit`). Raised events are queued and drained, so a chain of reactions runs to completion without unbounded recursion.
- All `inotify` sources share a single inotify instance; each event is routed to the right source by its watch descriptor. If the kernel drops a watch (its watched path was removed), only that source is marked *failed*; the others keep running.

## Configuration

Like [66-supervise](66-supervise.html), `66-eventd` has no configuration files of its own: it reads everything from each service's resolve database, compiled by [66 parse](66-parse.html) from the [frontend](66-frontend.html) file. The relevant compiled facts are:

- `Type = event` and its `EventType`: the service is a source of the given family.
- the `[Event]` rule (`EventType`, `From`, `On`/`OnAll`, `Do`, `Emit`, `Propagate`): the service is a reactor.

## Files

`66-eventd` manages the following entries in the scandir:

- `eventd/s`: the control socket on which the daemon receives arm/disarm/emit messages. It is restricted to the scandir owner.
- `eventd/s.lock`: guarantees that a single `66-eventd` serves the scandir.
- the `supervise/status` record of each `event` source — `66-eventd` writes it itself (a source has no supervisor): *done* when armed, *down* when disarmed, *failed* when its watcher dies. It is read by [66 status](66-status.html) like any other service's status.
- hidden scandir entries named `.<service>`: a tick source and an armed `Do = start` reactor live under a dotted name so [66-scandir](66-scandir.html) ignores them; `66-eventd` finds them again at startup.

## Anti-loop backstop

If a reactor fires **10 times within 10 seconds**, `66-eventd` treats it as a runaway loop and **squelches** it: the reactor is disarmed and destroyed, with a warning in the log. It is not throttled and does not re-arm itself — a fresh [66 start](66-start.html) is required. The backstop applies to `Emit` as well, which is what breaks a cyclic `Emit` chain. It is the event-system equivalent of the supervisor's crash budget.

`66-eventd` is itself supervised and may restart. On startup it **repopulates** its tables from the scandir, re-arming every supervised source and reactor best-effort; arming is idempotent, so rules survive a daemon bounce without operator action.

## Control

`66-eventd` listens on `eventd/s` for short messages — *arm*, *disarm*, and *emit a user event*. Only the scandir owner may connect (the daemon checks the peer's credentials and rejects the rest). You normally never write to this socket yourself: [66 start](66-start.html), [66 stop](66-stop.html) and [66 emit](66-emit.html) do it for you.
