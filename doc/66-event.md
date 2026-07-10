# The event system

The event system lets a service **react to something that happens elsewhere on the
system** by running a `66` command on itself — restart when a file changes, reload when a
signal reaches another service, start on a schedule, or start when a dependency dies. The
reaction is declared in the frontend service file and compiled into the service resolve
file at parse time. The daemon that runs the rules at runtime is
[66-eventd](66-eventd.html); this page documents how you *declare* them.

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
  [`[Event]`](#declaring-a-reactor-the-event-section) section. When its source fires, it
  runs a `66` command **on itself** and/or raises a named event.
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
  [`Emit`](#emit) key.

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
remaining keys depend on it.

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
[`On`](#on--onall) vocabulary applies. When the trigger fires the reactor performs a
[`Do`](#do), raises an [`Emit`](#emit), or both — at least one is required.

### EventType = service (reactor)

| Role | Section | Required keys | Optional keys |
|---|---|---|---|
| reactor | `[Event]` | `EventType`, `From`/`FromField`, `On`/`OnAll`, `Do`/`Emit` | — |

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
| reactor | `[Event]` | `EventType`, `From`/`FromField`, `On`/`OnAll`, `Do`/`Emit` | — |

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
A name is raised in two ways: by hand with `66 emit <name>`, or by another reactor's
[`Emit`](#emit) key. That second form is how reactions chain. In the pair below, `backend`
reacts to `rabbitmq` going down by stopping itself **and** raising `backend-down`; `alerter`
listens for that name:

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
| reactor | `[Event]` | `EventType`, `From`/`FromField`, `Do`/`Emit` | — |

A reactor to a configured source carries **no `On`**: the condition lives in the source and
is authoritative. To react differently, create a distinct source.

`EventType` is still stated explicitly here, even though [`From`](#from) already names the
source. It is redundant for these three families but kept on purpose: every reactor is then
uniform and self-describing — you read the family from the section itself, without having to
open the source's frontend.

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
| `inotify` | `Watch` + `On` | `From`/`FromField` + `Do`/`Emit` |
| `schedule` | `Expression` (+ `Timezone`) | `From`/`FromField` + `Do`/`Emit` |
| `timer` | `Every` | `From`/`FromField` + `Do`/`Emit` |
| `service` | *(not a source)* | `From`/`FromField` + `On`/`OnAll` + `Do`/`Emit` |
| `signal` | *(not a source)* | `From`/`FromField` + `On`/`OnAll` + `Do`/`Emit` |
| `user` | *(not a source)* | `On` + `Do`/`Emit` *(no `From`)* |

Every reactor carries `EventType` and at least one of `Do` / `Emit`. Every source carries
`[Main] Type = event`.

## Key reference

### EventType

**Source Snippet**:
```ini
EventType = service
```

Selects the family of event. In a source it appears in `[Main]`; in a reactor it appears in
the `[Event]` section. It is distinct from the [`[Main] Type`](66-frontend.html#type) key.

* mandatory: yes (!)

* syntax: [inline](66-frontend.html#inline)

* valid values: `service`, `signal`, `inotify`, `schedule`, `timer`, `user` — see the
  [synthesis table](#synthesis-keys-per-eventtype).

### Watch

**Source Snippet**:
```ini
Watch = /etc/resolv.conf
```

The filesystem path an `inotify` **source** watches, paired with [`On`](#on--onall).

* mandatory: yes for an `inotify` source; not valid elsewhere.

* syntax: [inline](66-frontend.html#inline)

* valid values: any absolute path to an existing file or directory.

### Expression

**Source Snippet**:
```ini
Expression = "0 0 3 * * ?"
```

The cron expression of a `schedule` **source**. The engine is a **Quartz-style** scheduler —
read the notes, it is **not** classic 5-field Vixie cron.

* mandatory: yes for a `schedule` source; not valid elsewhere.

* syntax: [quotes](66-frontend.html#quotes)

* valid values — a cron expression of **5, 6 or 7 space-separated fields**:

    ````
    [seconds] minutes hours day-of-month month day-of-week [year]
    ````

    | Fields | Layout |
    |---|---|
    | 5 | `min hour dom month dow` (seconds default to `0`) |
    | 6 | `sec min hour dom month dow` |
    | 7 | `sec min hour dom month dow year` |

    Ranges `0-59`/`0-59`/`0-23`/`1-31`/`1-12`/`0-7`/`1970-2200`. Months accept `JAN`..`DEC`,
    days accept `SUN`..`SAT` (case-insensitive, `0` = Sunday). Operators: `*` `,` `-` `/`
    plus Quartz `?` (no specific value), `L` (last), `L-<n>`, `LW` (last weekday), `<n>W`
    (nearest weekday), `<n>L` (last weekday-n), `<n>#<m>` (m-th weekday-n, `6#3` = 3rd
    Friday). Macros: `@yearly`/`@annually`, `@monthly`, `@weekly`, `@daily`/`@midnight`,
    `@hourly`, `@minutely`, `@secondly`.

* notes:

    * You **must** put `?` on either day-of-month or day-of-week — they cannot both carry a
      value. Even in 5 fields, `"0 3 * * *"` is **rejected**; write `"0 3 * * ?"`.
    * There is **no `@reboot`** macro.
    * The expression is validated at [66 parse](66-parse.html) time; an invalid one fails
      with a clear error rather than silently at runtime.

### Timezone

**Source Snippet**:
```ini
Timezone = Europe/Paris
```

The timezone the [`Expression`](#expression) of a `schedule` source is evaluated in.

* mandatory: no.

* syntax: [inline](66-frontend.html#inline)

* valid values: any IANA timezone name (`UTC`, `Europe/Paris`, …), up to 255 characters.
  When omitted, the schedule is evaluated in **UTC**.

### Every

**Source Snippet**:
```ini
Every = 30s
```

The period of a `timer` **source**: a relative, monotonic interval that fires again and
again, unaffected by wall-clock changes.

* mandatory: yes for a `timer` source; not valid elsewhere.

* syntax: [inline](66-frontend.html#inline)

* valid values: a positive whole number with an optional unit suffix — `s` (or none) for
  seconds, `m` minutes, `h` hours, `d` days. Examples: `30s`, `5m`, `1h`, `90`. The value
  must be at least one second.

### From

**Source Snippet**:
```ini
From = ( rabbitmq )
```

The source(s) a reactor subscribes to. **Always explicit** — sources are never inferred from
the `On` list.

* mandatory: yes for every reactor **except** `user`, unless [`FromField`](#fromfield) is
  used instead. At least one of `From` / `FromField` must be present.

* syntax: [brackets](66-frontend.html#brackets)

* valid values: the name of any valid service. For `service`/`signal` it is a supervised
  service; for `inotify`/`schedule`/`timer` it is the name of the `event`-type source.

### FromField

**Source Snippet**:
```ini
FromField = Depends
```

Turns this service's own dependency relationships into event sources, so you don't have to
restate them in [`From`](#from). Instead of naming each source, you point at one of the
service's existing dependency lists; the rule then reacts to whatever that list currently
holds. `66` records the *choice* of list in the resolve file (not a frozen copy of it), and
[66-eventd](66-eventd.html) **resolves it to concrete service names when it loads the rule**,
reading the service's current dependency list. The reaction therefore sees a plain list of
sources, with no indirection.

The benefit is that the event rule **follows the dependency list automatically**: add or
remove an entry later and the set of sources tracks it — no second place to keep in sync, and
no re-parse needed for the change to take effect.

* mandatory: no — but at least one of `From` / `FromField` must be present (except `user`,
  which is sourceless).

* syntax: [inline](66-frontend.html#inline)

* valid values:

    * `Depends` — the service's [`Depends`](66-frontend.html#depends) list: react to the
      services this one needs.
    * `RequiredBy` — its [`RequiredBy`](66-frontend.html#requiredby) list: react to the
      services that need this one (computed from the whole system graph).
    * `OptsDepends` — its [`OptsDepends`](66-frontend.html#optsdepends) list: react to the
      optional dependency that was actually selected.

    `From` and `FromField` are **unioned**, duplicates removed — use either, or both.

**Example — restart on any dependency going down.** `webapp` depends on `postgres` and
`redis`; the rule below restarts it whenever either one goes down, without naming them a
second time:

```ini
# frontend: webapp
[Main]
Type = classic
Description = "web application"
Depends = ( postgres redis )
[Start]
Execute = ( /usr/bin/webapp )
[Event]
EventType = service
FromField = Depends
On = ( down )
Do = restart
```

Here `FromField = Depends` reacts to `postgres` and `redis` — exactly as if you had written
`From = ( postgres redis )`, but resolved from the live `Depends` list. Add a third entry to
`Depends` and it becomes an event source too, automatically, with no re-parse.

**Example — union of `FromField` and `From`.** Combine the dependency list with an extra,
unrelated source:

```ini
[Event]
EventType = service
FromField = Depends
From = ( rabbitmq )
On = ( down )
Do = restart
```

Here the sources are `postgres`, `redis` **and** `rabbitmq`.

### On / OnAll

**Source Snippet**:
```ini
On    = ( down )
OnAll = ( auth:up db:up )
```

The trigger condition(s). Two keys select how several conditions combine:

* `On` — a single condition, or a bracketed list treated as **OR** (fires if any listed
  condition matches). Valid for every family that carries a condition.
* `OnAll` — an **AND**: fires only when all listed conditions hold at once. It is an AND over
  **current states**, so it is valid only for `service` and `signal` reactors; a `user`
  reactor — whose conditions are momentary emitted names, not states — uses `On` only.

Use exactly one of them.

* mandatory: yes for `service`, `signal`, `user` reactors and for the `inotify` **source**;
  forbidden for `inotify`/`schedule`/`timer` reactors.

* syntax: [brackets](66-frontend.html#brackets) — parentheses required, even for a single
  value: `On = ( down )`.

* valid values — depend on `EventType`:

    * **`service`** — a status **state** word — `down`, `starting`, `up`, `stopping`,
      `finishing`, `restarting`, `done`, `failed` — or a status **result** word — `success`,
      `exited`, `signaled`, `timeout-start`, `timeout-stop`, `crash-limit`, `exec-failed`. Two
      results take an argument: `exited:<code>` (a specific exit code) and `signaled:<SIG>`
      (a specific signal, e.g. `signaled:SIGKILL`). These are exactly the words `66 status`
      prints, so a rule reads the same as the state it reacts to. (`signaled` means *the
      process died from a signal*, unlike a `signal` reactor which means *a routed signal was
      received*.)
    * **`signal`** — a signal name, e.g. `SIGHUP`.
    * **`user`** — the emitted name, e.g. `backend-down`.
    * **`inotify` source** — one or more standard `inotify(7)` event names watched on
      [`Watch`](#watch): `IN_ACCESS`, `IN_MODIFY`, `IN_ATTRIB`, `IN_CLOSE_WRITE`,
      `IN_CLOSE_NOWRITE`, `IN_OPEN`, `IN_MOVED_FROM`, `IN_MOVED_TO`, `IN_CREATE`, `IN_DELETE`,
      `IN_DELETE_SELF`, `IN_MOVE_SELF`, plus the shorthands `IN_MOVE`
      (`IN_MOVED_FROM`+`IN_MOVED_TO`), `IN_CLOSE` (`IN_CLOSE_WRITE`+`IN_CLOSE_NOWRITE`) and
      `IN_ALL_EVENTS`. These are the kernel's own constants and map straight to the watch
      mask.

      Beware: `IN_CREATE`/`IN_DELETE`/`IN_MOVED_*` only fire for entries **inside** a watched
      directory, not for a watched file. A tool that replaces a file atomically (write-temp
      then rename — dhcpcd, certbot, most editors) does **not** raise `IN_MODIFY` on it; watch
      the directory (`IN_CREATE`/`IN_MOVED_TO`) or the file with `IN_MOVE_SELF`/
      `IN_DELETE_SELF`.

* per-source form: in a list, a bare token (`up`) applies to **all** sources of `From`; a
  `service:condition` token (`auth:up`) scopes the condition to one named source, which must
  be a member of `From`.

### Do

**Source Snippet**:
```ini
Do = restart
```

The `66` command the reactor runs **on itself** when the trigger fires.

* mandatory: no on its own, but a reactor must define at least one of `Do` / [`Emit`](#emit).
  A source never has a `Do`.

* syntax: [inline](66-frontend.html#inline)

* valid values: exactly one of `start`, `stop`, `restart`, `reload`, `reconfigure`, `free` —
  the matching `66` command ([66 start](66-start.html), [66 stop](66-stop.html),
  [66 restart](66-restart.html), [66 reload](66-reload.html),
  [66 reconfigure](66-reconfigure.html), [66 free](66-free.html)). Bare commands only, no
  argument.

### Emit

**Source Snippet**:
```ini
Emit = backend-down
```

Raises a `user` event of the given name when the trigger fires, **independently** of
[`Do`](#do). This is how reactions chain: another reactor with `EventType = user` and
`On = ( <name> )` will fire in turn (as would `66 emit <name>`). A reactor may carry `Do`,
`Emit`, or both.

* mandatory: no on its own, but a reactor must define at least one of [`Do`](#do) / `Emit`.
  A source never has an `Emit`.

* syntax: [inline](66-frontend.html#inline)

* valid values: any name. It matches the `On` of a `user` reactor.

## Notes and pitfalls

* **One rule per service.** A frontend holds at most one source *or* one reactor. To react
  to several unrelated events, split the logic across services, or chain them with `Emit`.
* **`signaled` vs `signal`.** `On = ( signaled )` on a `service` reactor fires when the target
  *dies from a signal*; a `signal` reactor fires when a signal is *routed to* the target by
  `66`. They are opposite directions.
* **Quartz `?`.** A `schedule` expression must carry `?` on day-of-month or day-of-week; it
  has no `@reboot`.
* **`Emit` chains can loop.** `Emit = a` triggering a rule whose `Emit = b` triggering a rule
  whose `Emit = a` is a cycle. `66-eventd` breaks such loops at runtime; still, keep chains
  short and acyclic.

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
FromField =
On = ()
OnAll = ()
Do =
Emit =
```
