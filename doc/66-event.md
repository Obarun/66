# The event system

The event system lets a service **react to something that happens elsewhere on the
system** by running a `66` command on itself — restart when a file changes, reload when a
signal reaches another service, start on a schedule, or start when a dependency dies. The
reaction is declared in the frontend service file and compiled into the service resolve
file at parse time. The daemon that runs the rules at runtime is
[66-eventd](66-eventd.html).

This page documents the **model** and its **runtime behaviour**. The per-field syntax —
which key goes in which section, its allowed values — is part of the frontend reference in
[66-frontend](66-frontend.html): the `Type = event` source keys live in
[`[Main]`](66-frontend.html#section-main), the reactor keys in the
[`[Event]`](66-frontend.html#section-event) section.

## A worked example

Say your DHCP client rewrites `/etc/resolv.conf`, and you want `dnsmasq` to restart every
time that happens. Two pieces are involved: a **source** that notices the file change, and
a **reactor** that acts on it.

First, the source — a service whose only job is to watch the file:

```ini
# frontend: resolv-watch
[Main]
Type = event
Description = "watch /etc/resolv.conf"
EventType = inotify
Watch = /etc/resolv.conf
On = ( IN_CLOSE_WRITE )
```

Then, `dnsmasq` itself gains an `[Event]` section that subscribes to that source:

```ini
# frontend: dnsmasq
[Main]
Type = classic
Description = "dnsmasq daemon"
[Start]
Execute = ( /usr/bin/dnsmasq -k )
[Event]
EventType = inotify
From = ( resolv-watch )
Do = restart
```

At runtime: [66 start](66-start.html) `resolv-watch` **arms** the watch; when
`/etc/resolv.conf` changes, `66-eventd` sees that `dnsmasq` has `From = ( resolv-watch )`
and runs `66 restart dnsmasq`. That is the whole model — a source emits, one or more
reactors act.

## Sources and reactors

Every event flows from a **source** to one or more **reactors**.

* A **reactor** is an ordinary service (`classic`, `oneshot` or `module`) that gains an
  [`[Event]`](66-frontend.html#section-event) section. When its source fires, it runs a
  `66` command **on itself** and/or raises a named event.
* A **source** is a service of the new [`event`](66-frontend.html#type) type. It runs no
  process; it only emits events. Its configuration lives **directly in `[Main]`**, there is
  no `[Event]` section. There are three families of source:
    * **inotify** — watch a filesystem path;
    * **schedule** — fire on a cron/calendar expression;
    * **timer** — fire on a relative interval.

Two more reactor families need no configured source at all:

* **service** — react to the up/down/crash transitions of any supervised service;
* **signal** — react to a signal routed by `66` to a supervised service;
* **user** — react to a name raised by [66 emit](66-emit.html), or by another reactor's
  `Emit` key.

### About the `event` type and arming

An `event` service is **not supervised**: it has no `[Start]` section and no running
process. On such a service, [66 start](66-start.html) and [66 stop](66-stop.html) are **not**
process supervision — they are *arm* and *disarm* messages sent to `66-eventd`. `66 start`
tells the daemon to begin watching (open the inotify watch, program the timer, …); `66 stop`
tells it to stop. Everything else (`enable`, dependencies, trees) works as usual.

### One rule at most, and it is opt-in

The event system is **entirely optional**. The overwhelming majority of services declare no
event rule at all. When a service does, it carries **at most one** rule: a service is either
one source, or one reactor — never both, and never several rules at once.

## Declaring a source (`[Main] Type = event`)

A source is a whole frontend of type `event`. The `EventType` key selects the family; the
remaining keys depend on it. Field syntax is documented in
[66-frontend](66-frontend.html#section-main).

### EventType = inotify (source)

| Role | Section | Required keys | Optional keys |
|---|---|---|---|
| source | `[Main]` | `EventType`, `Watch`, `On` | — |

```ini
# frontend: resolv-watch
[Main]
Type = event
Description = "watch /etc/resolv.conf"
EventType = inotify
Watch = /etc/resolv.conf
On = ( IN_CLOSE_WRITE IN_MOVE_SELF )
```

### EventType = schedule (source)

| Role | Section | Required keys | Optional keys |
|---|---|---|---|
| source | `[Main]` | `EventType`, `Expression` | `Timezone` |

```ini
# frontend: nightly-3am
[Main]
Type = event
Description = "fires every night at 03:00 Paris time"
EventType = schedule
Expression = "0 0 3 * * ?"
Timezone = Europe/Paris
```

### EventType = timer (source)

| Role | Section | Required keys | Optional keys |
|---|---|---|---|
| source | `[Main]` | `EventType`, `Every` | — |

```ini
# frontend: heartbeat
[Main]
Type = event
Description = "fires every 30 seconds"
EventType = timer
Every = 30s
```

## Declaring a reactor (the `[Event]` section)

A reactor is an ordinary supervised service that adds an `[Event]` section. The `EventType`
selects which family of source it subscribes to and, for `service`/`signal`/`user`, which
`On` vocabulary applies. When the trigger fires the reactor performs a `Do`, raises an
`Emit`, or both — at least one is required. Field syntax is documented in
[66-frontend](66-frontend.html#section-event).

### EventType = service (reactor)

| Role | Section | Required keys | Optional keys |
|---|---|---|---|
| reactor | `[Event]` | `EventType`, `From`, `On`/`OnAll`, `Do`/`Emit` | — |

```ini
# frontend: backend
[Main]
Type = classic
Description = "backend worker"
[Start]
Execute = ( /usr/bin/backend )
[Event]
EventType = service
From = ( rabbitmq )
On = ( down )
Do = stop
Emit = backend-down
```

`OnAll` fires only when several conditions hold at once — useful to wait on a whole set of
dependencies:

```ini
[Event]
EventType = service
From  = ( auth cache db )
OnAll = ( auth:up cache:up db:up )
Do    = restart
```

### EventType = signal (reactor)

| Role | Section | Required keys | Optional keys |
|---|---|---|---|
| reactor | `[Event]` | `EventType`, `From`, `On`/`OnAll`, `Do`/`Emit` | — |

```ini
# frontend: sshd
[Main]
Type = classic
Description = "OpenSSH daemon"
[Start]
Execute = ( /usr/sbin/sshd -D )
[Event]
EventType = signal
From = ( network )
On   = ( SIGHUP SIGUSR1 )
Do   = reload
```

### EventType = user (reactor)

| Role | Section | Required keys | Optional keys |
|---|---|---|---|
| reactor | `[Event]` | `EventType`, `On`, `Do`/`Emit` | — |

A `user` reactor has **no `From`** — it listens for a *name*, wherever that name comes from.
A name is raised in two ways: by hand with `66 emit <name>`, or by another reactor's `Emit`
key. That second form is how reactions chain. In the pair below, `backend` reacts to
`rabbitmq` going down by stopping itself **and** raising `backend-down`; `alerter` listens
for that name:

```ini
# frontend: backend
[Main]
Type = classic
Description = "backend worker"
[Start]
Execute = ( /usr/bin/backend )
[Event]
EventType = service
From = ( rabbitmq )
On   = ( down )
Do   = stop
Emit = backend-down
```
```ini
# frontend: alerter
[Main]
Type = oneshot
Description = "page the on-call engineer"
[Start]
Execute = ( /usr/local/bin/page-oncall )
[Event]
EventType = user
On = ( backend-down )
Do = start
```

The same `alerter` also fires if an operator runs `66 emit backend-down` from a script — the
`user` family is the general-purpose entry point into the event system.

### EventType = inotify / schedule / timer (reactor)

| Role | Section | Required keys | Optional keys |
|---|---|---|---|
| reactor | `[Event]` | `EventType`, `From`, `Do`/`Emit` | — |

A reactor to a configured source carries **no `On`**: the condition lives in the source and
is authoritative. To react differently, create a distinct source.

`EventType` is still stated explicitly here, even though `From` already names the source. It
is redundant for these three families but kept on purpose: every reactor is then uniform and
self-describing — you read the family from the section itself, without having to open the
source's frontend.

```ini
# frontend: backup  — runs every night, driven by the nightly-3am source above
[Main]
Type = oneshot
Description = "nightly backup"
[Start]
Execute = ( /usr/local/bin/backup )
[Event]
EventType = schedule
From = ( nightly-3am )
Do = start
```

## A cascading example: certificate rotation

Renewing a TLS certificate is a classic orchestration headache: several daemons must pick up
the new file, some can *reload* it in place, some must be fully *restarted*, and a front
proxy must come last so it never briefly serves the old certificate. With `Do` and `Emit`
this is a short chain.

The renewal tool (e.g. certbot) announces the event from its deploy hook:

```
66 emit cert-renewed
```

Each daemon reacts in the way that suits it — note that `nginx` **reloads** while `postfix`
**restarts**, decided per reactor:

```ini
# frontend: nginx
[Event]
EventType = user
On   = ( cert-renewed )
Do   = reload
Emit = nginx-reloaded
```
```ini
# frontend: postfix
[Event]
EventType = user
On = ( cert-renewed )
Do = restart
```

The front proxy must reload **after** nginx, so it does not chase `cert-renewed` directly —
it waits for the name nginx raises once its own reload is done:

```ini
# frontend: haproxy
[Event]
EventType = user
On = ( nginx-reloaded )
Do = reload
```

The result is a fan-out (`nginx` + `postfix` both react to `cert-renewed`) with a one-step
ordering constraint (`haproxy` after `nginx`), and each service keeps its own `reload`
-versus-`restart` decision — the very point where a single propagated “reload” command falls
short. Ordering across the chain is expressed by *who emits what*, so keep the chain shallow
and acyclic.

## Synthesis: keys per EventType

| EventType | Source (`[Main]`) | Reactor (`[Event]`) |
|---|---|---|
| `inotify` | `Watch` + `On` | `From` + `Do`/`Emit` |
| `schedule` | `Expression` (+ `Timezone`) | `From` + `Do`/`Emit` |
| `timer` | `Every` | `From` + `Do`/`Emit` |
| `service` | *(not a source)* | `From` + `On`/`OnAll` + `Do`/`Emit` |
| `signal` | *(not a source)* | `From` + `On`/`OnAll` + `Do`/`Emit` |
| `user` | *(not a source)* | `On` + `Do`/`Emit` *(no `From`)* |

Every reactor carries `EventType` and at least one of `Do` / `Emit`. Every source carries
`[Main] Type = event`. Each key's full definition — mandatory-ness, syntax and valid values
— is in [66-frontend](66-frontend.html#section-event).

## Runtime behaviour

Declaring a rule is only half the story; the other half is *when* and *whether* it actually
fires. `66-eventd` applies several rules that are easy to miss on paper. This section is the
authoritative description of them.

### Arming and disarming

For a `Type = event` **source**, [66 start](66-start.html) and [66 stop](66-stop.html) do not
supervise a process — they **arm** and **disarm** the source at the daemon (open the inotify
watch, program the timer or schedule).

For a **reactor** the two verbs are **not** symmetric, and that asymmetry is the single most
common point of confusion:

* [66 start](66-start.html) **arms** the reactor — from then on it reacts to its events.
* [66 stop](66-stop.html) brings the service **down but leaves the reactor armed**. The rule
  stays registered at `66-eventd` and keeps reacting, so a `Do = start` reactor you stopped
  **comes back up on its next event**. `stop` acts on the running service, never on the
  reaction.
* [66 free](66-free.html) — which stops the service *and* unsupervises it — is what
  **disarms** the reactor: it leaves the scandir and `66-eventd`, and reacts no more. For a
  `module`, a single `free` disarms every member reactor in the same pass.

In short, to make a reactor stop reacting you must **`free` it — a plain `stop` is not
enough**. This is also why *Recovery after `66-eventd` restarts* (below) re-arms a
merely-stopped reactor: it is still in the scandir, so the daemon picks it up again on the
next repopulate.

A reactor whose action is `Do = start` is a further special case: on `66 start` it is
**armed, not launched**. The parser forces such a service *down*, and starting it only
registers the rule — the service comes up later, when its event fires. In
[66 status](66-status.html) an armed-and-idle `oneshot`/`module` reactor shows the
**WAITING** state; a `classic` reactor simply shows *down*.

### When a reactor actually fires — state gating

When the trigger matches, `66-eventd` still checks the reactor's **current state** before
running its `Do`. The command is only issued when it would do something:

| `Do` | Fires only if the service is… |
|---|---|
| `start` | `down`, `done`, `failed` or `waiting` |
| `stop`, `restart`, `reload` | `up` |
| `reconfigure`, `free` | *(any state — always fires)* |

The most common surprise follows from the first row: **`Do = start` does not restart a
service that is already up** — it is a silent no-op (logged as *inhibited*). That is by
design: `start` means “bring it up”, and a healthy up service is already there. If the
reactor's own status cannot be read at that moment, every `Do` except `reconfigure`/`free`
is inhibited.

### Firing immediately on arm

Because each `From` source is also a **dependency** of the reactor, `66` brings the source
up before it arms the reactor. `66-eventd` therefore reads the source's *current* state at
arm time: a `service` reactor whose awaited condition **already holds** fires at once,
instead of waiting for the source's next transition. You do not have to arrange for the
event to happen strictly after the reactor is armed.

### `On` versus `OnAll`

`On` is an **OR**: the reactor fires as soon as one listed condition matches. `OnAll` is an
**AND** evaluated on a **point-in-time snapshot** of every source's committed status — it
fires only when all conditions hold *at the same instant*. A subtlety worth remembering: if
one source's state cannot be determined at that instant, the `OnAll` **fails** (an
unprovable condition is treated as unmet), not merely “not yet true”.

### `reload` is not a signal

This is the least intuitive rule. `66 reload` and a `Do = reload` action deliver the
service's reload-signal to its process, but they do **not** announce a signal to the event
system. Consequently **a reload never wakes a `signal` reactor** listening on, say,
`On = ( SIGHUP )` — even though the reload may itself send SIGHUP under the hood.

A `signal` reactor fires only for a signal **routed by `66`** ([66 signal](66-signal.html), or
a `Do` that maps to a signal) to a service that was **up** when the signal was routed. A
signal aimed at an already-down service announces nothing (it killed nothing).

### The in-flight latch

While a reactor's action is running — forked but not yet reaped — the reactor **absorbs every
re-trigger** until that action completes. This covers the short window between issuing the
command and the target's status being written back, so a burst of events cannot stack up a
pile of duplicate actions. The latch applies to `Emit`-only reactors too.

### The anti-loop backstop

If a reactor fires **10 times within 10 seconds**, `66-eventd` decides it is runaway and
**squelches** it: the reactor is disarmed and destroyed, with a warning in the log. It is
*not* throttled and it does **not** re-arm itself — a fresh [66 start](66-start.html) is
required to bring it back. The backstop also applies to `Emit`, which is what ultimately
breaks a cyclic `Emit` chain.

### `Emit` timing

A reactor that carries both `Do` and `Emit` raises its event **after** the `Do` has
committed (once the action process is reaped), not the instant the trigger matches. An
`Emit`-only reactor raises its event as soon as the filters pass.

### Tick sources at runtime

`timer`, `inotify` and `schedule` sources are started and stopped like any `Type = event`
service, but they are **not** supervised by the scandir and are **not** reference-counted by
the reactors that name them — each is armed by its own `start`. `66-eventd` writes their
status itself: *done* when armed, *down* when disarmed, *failed* when the underlying watcher
dies.

A source can die at runtime: if an `inotify` source's watched path is removed (the kernel
drops the watch), or a timer/schedule watcher errors, **that one source** goes *failed* while
every other source keeps ticking. Re-create the path and `66 start` it again to re-arm — the
death is not permanent.

### Cascading a disarm

Since a reactor **depends** on its `From` sources, `66 free <source>` (without `-P`)
propagates to the reactors that require it and **disarms them in cascade**. You free the
source, and the rules that fed on it go away with it.

### Recovery after `66-eventd` restarts

`66-eventd` is itself supervised and may restart (a crash, a scandir reconfigure). On
startup it **repopulates** from the scandir: it re-arms every supervised event source and
every reactor it finds, best-effort. Arming is idempotent, so nothing is double-armed. You
do not need to re-`start` your rules by hand after the daemon bounces.

## Notes and pitfalls

* **One rule per service.** A frontend holds at most one source *or* one reactor. To react
  to several unrelated events, split the logic across services, or chain them with `Emit`.
* **`signaled` vs `signal`.** `On = ( signaled )` on a `service` reactor fires when the target
  *dies from a signal*; a `signal` reactor fires when a signal is *routed to* the target by
  `66`. They are opposite directions.
* **`reload` does not wake a `signal` reactor.** See [`reload` is not a
  signal](#reload-is-not-a-signal) above — the single most common source of confusion.
* **`Do = start` on an up service is a no-op.** See [state gating](#when-a-reactor-actually-fires-state-gating).
* **Quartz `?`.** A `schedule` expression must carry `?` on day-of-month or day-of-week; it
  has no `@reboot`.
* **`Emit` chains can loop.** `Emit = a` triggering a rule whose `Emit = b` triggering a rule
  whose `Emit = a` is a cycle. `66-eventd` breaks such loops with the [backstop](#the-anti-loop-backstop);
  still, keep chains short and acyclic.

## Prototype

Source (in a `[Main] Type = event` service):

```
[Main]
Type = event
Description = ""
EventType =
Watch =
On = ()
Expression = ""
Timezone =
Every =
```

Reactor (an `[Event]` section added to a normal service):

```
[Event]
EventType =
From = ()
On = ()
OnAll = ()
Do =
Emit =
```
