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

### A reactor acts on itself — and on nothing else

This is the load-bearing rule of the whole model. A reactor's `Do` has **no target**: there
is no key naming the service to act upon, because the service acted upon is always the one
declaring the rule. When the event fires, `66-eventd` runs `66 <do> <the reactor>` — the
reactor's own name, never another's.

Nothing else in 66 can reach into a service this way, and it is the opposite of how reactive
control is usually built. The common shape is a separate trigger file that names the service
it must activate, or a hook or dispatcher script that restarts it from the outside; either
way the rule that acts on a service lives somewhere that service never mentions. In 66 the
direction is reversed: the service states what happens to it.

Three consequences follow, and they are the reason the model is worth using:

* **No action at a distance.** The frontend of a service lists everything event-driven that
  can happen to it. You never have to grep the rest of the system to find out what restarts
  it at 3 a.m.
* **Subscribing never edits someone else's file.** Adding a reactor touches exactly one
  frontend — its own. The source is not modified, and neither is any other subscriber. A
  package can ship a service that reacts to a system event without patching a single file it
  does not own.
* **Watching is not acting.** Naming a source in `From` observes it; it never starts, stops
  or otherwise touches that source — see [`From` is a watch, not a
  dependency](#from-is-a-watch-not-a-dependency).

The one outward-facing key is [`Emit`](#emit), and it is deliberately indirect: it raises a
*name*, not a command, and it acts on nobody. Whoever wants to react to that name subscribes
to it, in their own frontend, with their own `Do`. So even the cascades below are made of
services that each decided for themselves — no service is ever acted upon by a rule it does
not carry.

### One source, many reactors

A source is **fan-out**: any number of reactors may name the same source in their `From` (or,
for `user`, key on the same name), and each reacts in its own way — its own `Do`, its own
`Emit`, its own `On` filter. One `timer` can drive a backup *and* a metrics flush *and* a log
rotation; one `IN_MOVED_TO` on a certificate directory can reload every daemon that serves it;
one `66 emit cert-renewed` reaches every subscriber at once. The source knows nothing about its
reactors — you add a subscriber, never touch the source.

This is where the model departs from unit-per-trigger designs. Where a trigger file names the
single service it activates, driving several services from one trigger means interposing a
group that pulls them all in, and giving each of them a *different* action is not expressible
at all — the trigger and the acted-upon service are 1:1. In 66 the coupling is 1:N by
construction, and each reactor keeps its own verb.

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
| reactor | `[Event]` | `EventType`, `From`, `On`/`OnAll`, `Do`/`Emit` | `Propagate` |

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
OnAll = ( auth:up cache:down db:up )
Do    = restart
```

### EventType = signal (reactor)

| Role | Section | Required keys | Optional keys |
|---|---|---|---|
| reactor | `[Event]` | `EventType`, `From`, `On`/`OnAll`, `Do`/`Emit` | `Propagate` |

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
| reactor | `[Event]` | `EventType`, `On`, `Do`/`Emit` | `Propagate` |

A `user` reactor has **no `From`** — it listens for a *name*, wherever that name comes from.
A name is raised in three ways: by hand with `66 emit <name>`, by another reactor's `Emit`
key, or by 66 itself at a system milestone (see [Lifecycle events raised by
66](#lifecycle-events-raised-by-66) below). The second form is how reactions chain. In the pair below, `backend` reacts to
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

### Lifecycle events raised by 66

66 raises a few `user` events on its own, at the moments in the system's life that a service
most often needs to hang off. You subscribe to them exactly like any other `user` name — an
`[Event]` section with `EventType = user` and `On = ( <name> )`. You never emit them
yourself with `66 emit`; 66 does, from inside `66 boot`, the shutdown daemon and
[`66 env`](66-env.html).

These names all carry a **dot**. The dotted form is reserved for events 66 raises itself; the
names you raise with `66 emit` or `Emit` are bare (`cert-renewed`, `backend-down`). The two
namespaces never collide, and a dot in an `On` line tells you at a glance the event comes from
the system, not from another service.

| Event | Raised when | Typical use |
|---|---|---|
| `boot.done` | boot has finished — every enabled tree is started | bring up something that must wait for a fully-booted system |
| `boot.failed` | boot could not start every enabled tree | raise an alert, open an emergency shell |
| `shutdown.begin` | a shutdown or reboot has been scheduled, **before** any service is stopped | flush state, notify a peer, quiesce a daemon cleanly |
| `env.<variable>` | [`66 env`](66-env.html) published *variable* — `env.DISPLAY`, `env.XAUTHORITY` | start a service the moment the value it needs becomes available |
| `unenv.<variable>` | [`66 env`](66-env.html) withdrew *variable* | tear a service down when the value it depends on goes away |

`boot.done` fires **after** the last enabled tree is up, which is exactly the hook a getty
wants — a login prompt should appear only once the machine has finished booting. Instead of
wiring the getty into the boot dependency graph, you subscribe it to the event:

```ini
# frontend: tty1
[Main]
Type = classic
Description = "getty on tty1"
[Start]
Execute = ( execl-cmdline -s { agetty 38400 tty1 } )
[Event]
EventType = user
On = ( boot.done )
Do = start
```

At boot the getty is **armed but not started** — a `Do = start` reactor is [armed
idle](#arming-and-disarming): it waits at rest and runs its `Execute` only when its event
arrives. When `boot.done` fires, 66 starts it. Because a source is fan-out, every getty and
every other "wait for boot" service subscribes to the same `boot.done` without boot having to
know they exist.

`shutdown.begin` is the mirror image, and its timing matters: it fires while services are
still up, during the shutdown grace period, so a reactor still has a live system to act on.
That window is short — keep the reaction quick (a flush, a signal), not a long job that would
be cut off when the teardown proceeds.

### EventType = inotify / schedule / timer (reactor)

| Role | Section | Required keys | Optional keys |
|---|---|---|---|
| reactor | `[Event]` | `EventType`, `From`, `Do`/`Emit` | `Propagate` |

A reactor to a configured source carries **no `On`**: the condition lives in the source and
is authoritative. To react differently, create a distinct source.

`EventType` is still stated explicitly here, even though `From` already names the source. It
is redundant for these three families but kept on purpose: every reactor is then uniform and
self-describing — you read the family from the section itself, without having to open the
source's frontend.

That redundancy is checked, not assumed: `66 parse` refuses a reactor whose `EventType`
disagrees with the family of the source named in `From`, and refuses a `From` that is not a
`Type = event` source at all. This is not cosmetic — `EventType` is the runtime dispatch key,
so a mismatch would arm cleanly and then never fire, silently.

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

A reaction is the bare `66` command, dependency chain included. A rule that must stay narrow
opts out with [`Propagate`](66-frontend.html#propagate):

```ini
# udevd frontend: reloads itself when a rules file changes, alone
[Main]
Type = classic
Description = "device manager"
[Start]
Execute = ( /usr/bin/udevd )
[Event]
EventType = inotify
From = ( udev-rules-watch )
Do = restart
Propagate = false
```

`udevd` is required by most of the boot. Without that last line, editing one rules file would
restart nearly the whole system; here only `udevd` bounces.

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

## Case studies: reactive control other init systems make hard

Every mainstream init and supervision system expresses a **static dependency graph** — start
order, "A needs B" — well. The part with no clean native form elsewhere is the **reactive**
one: *at runtime, react to something that happens to one service by acting on another.* The
cases below are each reproduced and verified on 66; each closes with an exact note on how other
systems fare, so the comparison is concrete rather than rhetorical.

### React to a daemon that gives up for good

A daemon exhausts its crash budget — [`MaxDeath`](66-frontend.html#maxdeath) deaths within a
[`MaxDeathInterval`](66-frontend.html#maxdeathinterval) window (milliseconds) — and the
supervisor marks it **failed**. You want to record it, or start a standby, once:

```ini
# frontend: flaky
[Main]
Type = classic
Description = "a daemon that keeps crashing"
[Start]
Execute = ( flaky-daemon )
MaxDeath = 3
MaxDeathInterval = 30000
```
```ini
# frontend: alerter
[Main]
Type = oneshot
Description = "record the failure, once"
[Start]
Execute = ( /usr/local/bin/alert "flaky is down for good" )
[Event]
EventType = service
From = ( flaky )
On   = ( failed )
Do   = start
```

`66 start alerter` arms the reactor and brings `flaky` up (its `From` source). `flaky` dies, is
retried, dies again; on the third death in the window the supervisor gives up and marks it
**failed**. `alerter` fires **once** and `flaky` **stays failed** — the reactor *watches*
`flaky` without depending on it, so its `Do = start` acts on `alerter` alone and never revives
`flaky` (see [`From` is a watch, not a dependency](#from-is-a-watch-not-a-dependency)).

**Compared with others.** A failure hook that only fires on a *terminal* failed state is blind
to the common case: with automatic restart enabled, a crash that is retried never reaches that
state, so a service that crashes and is restarted all day long triggers nothing. Hooks of that
shape also tend to de-duplicate on the failure edge, running once and staying silent while the
service keeps flapping. s6 and runit have no failure-to-action mechanism at all — the default is
to restart forever, and any cross-service reaction is a hand-written wrapper. 66 exposes both
the terminal `failed` and every intermediate `down` transition as reactable events, and a
reactor **re-fires** each time.

### Restart a client to reconnect when its backend restarts

When a shared backend — dbus, a database, a message broker — restarts, clients hold dead
connections and must be bounced to reconnect:

```ini
# frontend: client
[Main]
Type = classic
Description = "reconnects when the backend restarts"
[Start]
Execute = ( the-client )
[Event]
EventType = service
From = ( backend )
On   = ( up )
Do   = restart
```

Whenever `backend` reaches `up` — an operator restart, or the backend recovering on its own —
`client` restarts to reconnect. The two are decoupled in the dependency graph (`66 restart
backend` does not touch `client`, and `client`'s restart does not drag `backend` back up); the
coupling is purely the event rule.

**Compared with others.** The usual answer is *pre-declared propagation* rather than reaction:
a dependent is restarted when an operator restarts the named service, or is stopped when the
service it is bound to stops — but nothing brings it back when that service returns, which is
precisely the moment a client needs to reconnect. That is why restarting a shared bus is
routinely described as leaving its clients broken until they are bounced by hand. runit has no
mechanism — you script it (`sv hup <dependent>` in the dependency's `finish`); s6-rc
re-evaluates dependencies only at database-update time. None offers a lightweight "when the
backend is available again, restart me."

### Reload on an atomic config or certificate swap

Config and certificate tools update a file **atomically**: they write a temporary file in the
same directory and `rename()` it into place, so a reader never sees a half-written file. You
want to reload when the real file is replaced:

```ini
# frontend: cfg-watch  — the source
[Main]
Type = event
Description = "watch a config directory for an atomic replace"
EventType = inotify
Watch = /etc/myapp
On = ( IN_MOVED_TO )
```
```ini
# frontend: myapp  — the reactor
[Main]
Type = classic
Description = "reload on config change"
[Start]
Execute = ( myapp )
[Event]
EventType = inotify
From = ( cfg-watch )
Do = reload
```

A `rename()` into `/etc/myapp` raises `IN_MOVED_TO`, which fires the reload; because the mask is
explicit, an unrelated in-place write does **not**. (`rename()` is atomic only within one
filesystem — which is exactly why the temporary file must live in the same directory as its
target.)

**Compared with others.** A file watcher that binds to the *file's own* inotify descriptor
instead of its directory's `IN_MOVED_TO` does **not** fire on an atomic rename-into-place — it
misses the very pattern every safe config update uses, since the new file is a different inode.
66 lets you name the mask and the directory yourself, so the swap is exactly what you watch.
Any inotify-based watcher, 66 included, inherits the kernel's limits — a change made on a
remote NFS mount is not seen. s6, runit and OpenRC have **no** built-in file watching at all;
you bolt an `inotifywait` loop onto a manual `SIGHUP`.

### One event, per-service policy: certificate rotation

The [certificate-rotation cascade](#a-cascading-example-certificate-rotation) above is the
fourth case: one `66 emit cert-renewed` drives a fan-out where `nginx` **reloads** and `postfix`
**restarts** — each reactor picking its own verb — with a one-step ordering constraint carried
by `Emit`.

**Compared with others.** Propagation mechanisms carry a *single* verb — typically reload —
so a group where some daemons reload and others must restart cannot be driven from one trigger.
In practice the whole per-service matrix ends up encoded in the renewal tool's deploy hook,
far away from the services it acts on and owned by whoever maintains that hook. 66 keeps each
reactor's policy on the consumer, where it belongs.

### `From` is a watch, not a dependency

The first two cases rely on a single property, worth stating on its own. For a
`service`/`signal` reactor, **`From` is a subscription, not a runtime dependency**:

* At *arm* time it behaves like an ordering edge — 66 supervises the source before it arms the
  reactor, so the daemon exists for the reactor to read its current state (this is what lets a
  condition that already holds fire at once; see [firing immediately on
  arm](#firing-immediately-on-arm)).
* At *runtime* it carries **no propagation**: stopping or restarting the source does not tear
  the reactor down, and a reaction's `Do = start`/`restart` starts the reactor **without**
  re-pulling the source. That is why the direct rules above act on their source without the
  reactor ever reviving or dragging it.

A **tick** reactor (`inotify`/`schedule`/`timer`) is different: there the source is a passive
`Type = event` watch that exists only to serve its reactors, so `From` *is* a dependency — the
watch is armed with the reactor. A **`user`** reactor has no `From` at all; it keys on an
`Emit`ted or [`66 emit`](66-emit.html)ted name, which is the way to react to a source you must
not couple to.

One more timing note, independent of all this: a reactor's `Execute` runs when the reactor is
**armed** (`66 start`), not when its condition fires — except for a `Do = start`/`restart`
reactor, which is [armed idle](#arming-and-disarming) and runs its `Execute` only when the event
arrives (see [`Emit` timing](#emit-timing)). Put the work that must happen *at* the event in a
`Do = start`/`restart` reactor, not in the `Execute` of a plain source-watcher.

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

A reactor whose action is `Do = start` or `Do = restart` is a further special case: on
`66 start` it is **armed, not launched**. Both verbs *bring the service up* when the event
fires, so the parser forces such a service *down* and starting it only registers the rule —
the service comes up later, when its event fires. Every other verb is the opposite: `stop`,
`reload`, `reconfigure` and `free` act on a *running* service, so `66 start` launches the
reactor normally and it reacts from **up**. In [66 status](66-status.html) an armed-and-idle
reactor shows the **WAITING** state.

### When a reactor actually fires — state gating

When the trigger matches, `66-eventd` still checks the reactor's **current state** before
running its `Do`. The command is only issued when it would do something:

| `Do` | Fires only when the service is… |
|---|---|
| `start` | anything **except** `up` or `done` |
| `stop`, `reload` | anything **except** `down` or `failed` |
| `restart` | *(any state — always fires)* |
| `reconfigure`, `free` | *(any state — always fires)* |

The gate is built from two sets: a service is **active** when it is `up` or `done`, and
**inert** when it is `down` or `failed`; the five transitional states (`starting`,
`stopping`, `finishing`, `restarting`, `waiting`) are neither. `start` fires only on a
**non-active** service, `stop`/`reload` only on a **non-inert** one, and `restart` fires
unconditionally — it is the verb to use when the action must run on *every* matching event
regardless of where the service currently sits.

The most common surprise follows from the `start` row: **`Do = start` does not run on a
service that is already `up` or `done`** — it is a silent no-op (logged as *inhibited*),
because `start` means “bring it up” and an already-active service is there. This does
**not** keep a `oneshot`/`module` reactor from reacting repeatedly: such a reactor
**re-arms to `waiting` after each firing** — it never parks in `done` — so a `Do = start`
or `Do = restart` reactor fires again on every matching event, running its `Execute` exactly
once per event. If the reactor's own status cannot be read at that moment, every `Do` except
`reconfigure`/`free` is inhibited.

Parking in `waiting` does not lose the firing that just happened: the date of the last run
is kept beside the state, so [66 status](66-status.html) tells a reactor that already did
its work from one still waiting for its first event:

```
$ 66 status numlockx
Status : enabled, waiting since 1h 13min by event (last run 1h 13min ago)

$ 66 status
├─numlockx (pid=waiting(done), state=Enabled, type=oneshot, tree=global)
```

A reactor that has never fired since it was armed reads `(never run)`, and shows a bare
`waiting` in the list. [66 stop](66-stop.html) clears that memory, as does a reboot.

### Firing immediately on arm

Because 66 **supervises each `From` source before it arms the reactor** — as an establishment
edge for a `service`/`signal` reactor, or as a dependency for a tick reactor (see [`From` is a
watch, not a dependency](#from-is-a-watch-not-a-dependency)) — `66-eventd` reads the source's
*current* state at arm time: a `service` reactor whose awaited condition **already holds** fires
at once, instead of waiting for the source's next transition. You do not have to arrange for the
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

### Freeing a source

For a **tick** source (`inotify`/`schedule`/`timer`), which its reactors depend on, `66 free
<source>` (without `-P`) propagates to the reactors that require it and **disarms them in
cascade** — you free the source, and the rules that fed on it go away with it.

A **`service`/`signal`** reactor only *watches* its `From` service (see [`From` is a watch, not
a dependency](#from-is-a-watch-not-a-dependency)), so freeing that service does **not** disarm
the reactor: the rule stays armed on a now-absent source — it simply never fires again — until
you [`66 free`](66-free.html) the **reactor** itself. Disarming is always done on the reactor.

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
* **`Do = start` on an already-active (`up` or `done`) service is a no-op.** But an armed
  `oneshot`/`module` reactor re-arms to `waiting` between events, so this does *not* stop it
  firing again — it runs its `Execute` once per event, and `66 status` still reports when it
  last ran. See [state gating](#when-a-reactor-actually-fires-state-gating).
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
Propagate =
```
