# Changelog for 66

# In 0.9.1.0

## Overview

A small release: one new frontend key and a handful of fixes, several of which
concern the event system and the way a command decides what state a service must
end in.

The migration is **automatic** and touches one thing only, the `[Event]` addon of
your reactors, which gains a key. Nothing else in the resolve database changes, and
**no behaviour of yours changes**: the new key defaults to what your reactors already
did.

## What you must do

Nothing. Read the `Propagate` entry below if you run a reactor whose action restarts
a service that much of the system depends on, which is the case the key was made for.

## New things to explore

- **`[Event] Propagate`** ([66-frontend](66-frontend.html#propagate)):

    Whether a reactor's [`Do`](66-frontend.html#do) command walks the dependency
    chain. It defaults to `true`, because `Do = X` means what `66 X` means: a
    reactor is a trigger, not a variant of the command, so your existing reactors
    are untouched.

    Set it to `false` and the action carries `-P`. This is what makes a narrow
    reaction usable: an `inotify` reactor reloading `postgresql` when
    `pg_hba.conf` changes would otherwise drag every service that depends on the
    database, and their cascades, with it, SIGHUP-ing the whole application
    stack for one edited access control line.

    Which way the chain is walked depends on the verb, and so does what `false`
    costs you. `stop`, `restart`, `reload`, `reconfigure` and `free` reach the
    services that **depend on** the reactor. `start` instead reaches what the reactor
    **depends on**, and turning propagation off there starts it without bringing its
    dependencies up, which is rarely what you want.

## Bug fixes

- **`66 reconfigure` gives every service back the state it was found in** (d4f92e7):

    A service the user had stopped came back **started**; so did a service that had
    never been supervised and was only pulled in as a dependency. The command now
    takes a snapshot of each service before tearing anything down, and restores
    exactly that: what was running is started again, what was merely supervised is
    supervised again and left down, and what was not supervised is left alone.

- **A reactor that was running is running again after a reconfigure** (ae91b3b3):

    It used to fall back to armed-and-idle. Whether a service must merely be able to
    serve or must actually be up is now carried per service, so the reactor's own
    state decides.

- **`66 reload` and `66 signal` no longer behave like a stop** (bb55e971):

    Both inferred their intent from the signal character they were handed, and
    anything that was not `u` fell in the stop branch. A reload dropped a `down` file
    in the scandir, marking a running service as one not to bring back up, used the
    stop timeout, and **disarmed an event source** it was merely signalling.

- **A `Do = reconfigure` reactor no longer disarms itself** (e388b444):

    Its own action unsupervises the service in passing, and the disarm that reached
    the daemon was taken for a request to disappear: the reactor never reacted again.

- **`requiredby` of a leaf service is no longer empty** (ddb6118):

    A service declaring no dependency of its own owns no addon on disk, and the
    first string written to it landed at the offset that means *unset*. The count
    was right, the list read empty, on every leaf of a dependency tree.

- **A module member is found by its bare name** (52dde235, 8e907276):

    Inside a module, a service naming a sibling, in `Depends` or in the `From`/`On`
    of an `[Event]` section, is now resolved against the module namespace as well.

---

# In 0.9.0.0

- Adaptation to `oblibs` `0.4.0.0`

## Overview

This is a ***Major release***. It completes the direction announced at
`0.7.0.0`: **66 is now an independent service manager**. It no longer builds against
`skalibs`, `s6` or `execline` — the scanner, the supervisor, the logger, the fd
holder and the privilege dropper are native programs of the suite. This release
also brings the **event system** promised on the roadmap, and rewrites the
**boot and shutdown procedures in C**.

The ecosystem migration is **automatic**: the first `66` command run after the
upgrade converts your system and takes a snapshot beforehand. **You do not have
to convert anything by hand.**

What you *do* have to look at is your own files: a few frontend keys moved or
disappeared, the `init.conf` skeleton lost several keys, the `rc.*` skeleton
scripts are gone, and **the boot no longer needs to be told which tree to
start** — which may make one of your own boot services redundant.

This page is organised around that: **what you must do**, then **what happens by
itself**, then the rest.

---

## What you must do

A short checklist. Everything here is detailed further down. The first four are
**silent** — nothing errors out, the behaviour simply changes under you.

1. **If your `init.conf` sets `CONTAINER=1`**, the key is gone and is now
   ignored: your container **boots as bare metal**. Use `66 boot -c` instead.
2. **If a frontend uses `[Logger] Destination`**, the key is gone and is now
   ignored: the service **silently logs to the default directory**. Replace it
   with `[Execute] StdOut = 66log:/path`.
3. **If you customized `rc.init`, `rc.init.container`, `rc.shutdown` or
   `rc.shutdown.final`**, those files are gone and are no longer read: your
   changes have **no effect**. See [Your boot](#your-boot).
4. **If one of your boot services starts the enabled trees** — anything calling
   `66 tree start` on trees other than the boot tree — **66 now does that
   natively at boot**. Remove it, or it runs twice. See
   [How 66 boots now](#how-66-boots-now).
5. **Update your frontends** to silence the deprecation warnings: keys that moved
   section, `Build`, `TimeoutStart`/`TimeoutStop`, `s6log`. All of them still
   work and warn — this one is not urgent, but the old spellings will be removed
   in a future release.
6. **Reboot.** The migration rewrites your on-disk state; a running `0.8.2.x`
   scandir keeps its old picture until then.
7. **Packagers**: see [For packagers](#for-packagers) — the build dependencies
   and several meson options changed.

---

## Your frontend files

### Keys recentered on their section

Several `[Main]` keys that actually describe a *transition* or the process
*execution* have moved to the section of their domain. The former names
`TimeoutStart` / `TimeoutStop` merge into a single `Timeout` key, since the
section now disambiguates the start/stop meaning.

| Key | Was | Now declare in | Rename |
| --- | --- | --- | --- |
| `Notify` | `[Main]` | `[Start]` | — |
| `DownSignal` | `[Main]` | `[Stop]` | — |
| `StdIn` / `StdOut` / `StdErr` | `[Main]` | `[Execute]` | — |
| `TimeoutStart` | `[Main]` | `[Start]` | → `Timeout` |
| `TimeoutStop` | `[Main]` | `[Stop]` | → `Timeout` |
| `TimeoutStart` | `[Logger]` | `[Logger]` | → `Timeout` |

**Nothing breaks.** The old placements and the old names are still accepted:
they warn at parse time and will be removed in a future release. If a key is
declared both at its deprecated location and at its canonical section, the
**canonical section wins**.

For example, this still parses and warns:

```
[Main]
Type = classic
TimeoutStart = 5000
Notify = 3
StdOut = 66log
```

and becomes:

```
[Main]
Type = classic

[Start]
Timeout = 5000
Notify = 3

[Execute]
StdOut = 66log
```

One thing that *is* now refused: swapping a `[Start]` key into `[Stop]` or the
reverse. `TimeoutStart` or `Notify` in `[Stop]`, and `TimeoutStop` or
`DownSignal` in `[Start]`, are a parse error. Their deprecated `[Main]` spelling
is still accepted (as above); only the Start/Stop mismatch is fatal.

### Keys removed

An unknown key is only **ignored with a warning** — it is not a parse error. So
a frontend still using these keeps working, minus the behaviour you asked for.
Check your frontends.

| Key | What now |
| --- | --- |
| `[Logger] Destination` | **Gone.** Your custom log directory is silently ignored and the service logs to the default. Use `[Execute] StdOut = 66log:/path`. |
| `[Logger] TimeoutStop` | **Gone.** A logger has a single run transition; use `[Logger] Timeout`. |

`Destination` was deprecated-but-honored at `0.8.2.0` — it was automatically
converted for you. That conversion is over.

### `Build` is deprecated and ignored — the shebang decides

`Build` is still parsed in `[Start]`, `[Stop]` and `[Logger]`, but it does
nothing and warns. The build type is now **auto-detected**: a script is `custom`
if and only if the first non-blank line of `Execute` starts with a shebang
(`#!`); otherwise it is built as an execline script.

Two consequences if you relied on `Build`:

- `Build = custom` with an `Execute` carrying **no** shebang used to be a hard
  error. It is now silently built as **execline**.
- `Build = auto` (or no `Build` at all) with a shebang in `Execute` used to be
  built as execline. It is now auto-detected as **custom**.

In both cases, declaring the shebang you actually want is the fix.

### `s6log` becomes `66log`

The `s6log` value of `StdIn` / `StdOut` / `StdErr` is renamed **`66log`**, since
the logger is now `66-log`. `s6log` is still accepted, converted at parse time
with a warning, keeping your destination. Frontend files are **not** rewritten on
disk — edit them yourself to silence the warning.

### `MaxDeath` becomes a crash budget, and `MaxDeathInterval` is new

`MaxDeath` no longer means the number of death events the supervisor remembers.
Together with the new `MaxDeathInterval` key it now expresses a **crash budget**:
how many times a service may die within a time window before 66 gives up,
declares it `failed` and stops restarting it. Both are now declared in `[Start]`
— still accepted, deprecated, in `[Main]`.

| | `0.8.2.2` | `0.9.0.0` |
| --- | --- | --- |
| `MaxDeath` | death events remembered | deaths allowed inside a `MaxDeathInterval` window |
| accepted range | `0`–`4096` | **`0`–`16`** |
| default | `5` | `5` |
| `MaxDeath = 0` | — | **infinite restart, never *failed*** |
| `MaxDeathInterval` | — | **new**, milliseconds, default **`30000`** |

**If a frontend declares `MaxDeath` above 16, it no longer parses.** Everything
else keeps its meaning: the default budget is 5 deaths in 30 seconds.

Only an intrinsic *run-then-die* consumes the budget: a commanded stop never
counts, and a failed `exec` is reported as `exec-failed` with a progressive
backoff without consuming the budget. An exhausted budget gives state `failed`
with result `crash-limit`; a new `66 start` resets the counter.

### The logger default timestamp is honored again

A frontend that omits `Timestamp` in `[Logger]` used to produce
**untimestamped** log lines, ignoring the compile-time default. It now produces
**ISO** timestamps. `Timestamp = none` still yields no timestamp; `tai` and
`iso` were always honored — only the unset case was broken. **Expect timestamps
to appear in logs where you had none**, if you never set the key.

### `execl-envfile` rejects newline and NUL in a value

A value decoding to a newline (`\n`, `\012`, `\x0a`) or a NUL (`\0`, `\000`,
`\x00`) is now a **syntax error**. The environment is held as a NUL-separated
list and re-emitted as newline-separated text; neither byte survives the round
trip, so the parse now fails loudly instead of corrupting the value silently.
Note this **diverges from execline's `envfile`**, which permits a newline in a
value.

---

## Your boot

### How 66 boots now

This is the most visible behaviour change of the release, and the one most
likely to affect services you wrote yourself.

**Before**, the boot exec'd the `rc.init` skeleton script, which started **one
tree**, the one named by `init.conf`'s `TREE` key, `boot` by default. Starting
the *other* trees, the enabled ones, was somebody else's job: typically a service
inside the boot tree that called `66 tree start` on them once the boot tree was
up.

**Now**, `66 boot` does the whole thing itself, in memory, with no script and no
intermediate `66` process. Stage 2 brings the system up in two waves:

1. **the boot trees**: every tree belonging to the `boot` *group* (or the single
   tree named by `TREE`, if you set it — see below);
2. **then every enabled tree.**

So if you have a service whose job is to start the enabled trees, **it is now
redundant and does the work a second time**. Look for anything in your boot tree
that calls `66 tree start` on trees other than the boot tree, and remove it. This
is worth checking whatever your setup: the service is likely to be provided by
your distribution rather than written by you.

Two more consequences of the boot running in-process:

- **You no longer name the tree to boot.** Put your trees in the `boot` group
  (`66 tree admin -o groups=boot <tree>`, or at creation) and 66
  brings them all up, whatever they are called. No tree in the group is a no-op,
  not an error.
- **`66 status` now reports `by boot`** for services started at boot. Before,
  `rc.init` exec'd a fresh `66`, which lost the context, and boot-started
  services were recorded as `by user`.

A tree in the `boot` group cannot be enabled, so the two waves never overlap.

### `TREE` is still there and now optional

`TREE` is **not removed**. It is still read from `init.conf` **and from the
kernel command line**. What changed is that the shipped skeleton no longer sets
it, so it now **defaults to empty**, and empty means *group mode*.

| `TREE` | Boot starts |
| --- | --- |
| unset (the new default) | every tree in the `boot` group, then every enabled tree |
| `TREE=boot` (or any name) | that one tree, then every enabled tree |

**An existing `init.conf` that still says `TREE=boot` keeps working exactly as
before.** Nothing forces you to move. But it pins the boot to that single tree
by name and opts you out of the group mode, so leaving it set is a choice, not
an oversight. Clear the key to opt in.

Both forms start the enabled trees in the second wave, that part is
unconditional.

### `init.conf`

| Key | Change |
| --- | --- |
| `VERBOSITY` | default **`1` → `2`** |
| `TREE` | **removed from the skeleton**, still honored, now empty by default |
| `CONTAINER` | **removed** — use `66 boot -c` |
| `RCINIT_CONTAINER` | **removed** |
| `RCINIT` | **removed** |
| `RCSHUTDOWN` | **removed** |
| `RCSHUTDOWNFINAL` | **removed** |
| `RESCAN` | **removed** (it was never wired to anything) |
| `LIVE`, `PATH`, `UMASK`, `CATCHLOG` | unchanged |

No key was added or renamed.

**A stale `init.conf` still listing the removed keys is not an error — the keys
are ignored, without a warning.** That is fine for five of them, but **`CONTAINER=1`
is a trap**: it used to select the container init, it now does nothing at all, so
the system **silently boots as bare metal**. Pass `66 boot -c` instead.

Every key is now optional, and an absent key keeps its built-in default.
Previously `PATH`, `TREE` and `LIVE` were exported to stage 2 from buffers that
stayed **empty** when the key was absent.

The default verbosity moves to `2` so that the warnings the internal daemons
emit, for instance an event reactor stopped by the anti-loop backstop, are
visible without reconfiguring anything.

### The `rc.*` skeleton files are gone

| Removed | Replaced by |
| --- | --- |
| `rc.init` | `66 boot` stage 2, in memory |
| `rc.init.container` | the same stage 2; a container is a branch selected by `66 boot -c` |
| `rc.shutdown` | `66-shutdownd` stage 3, in memory |
| `rc.shutdown.final` | the final stage, now compiled C |

**If you customized any of these, your changes are silently ignored**. The files
are no longer read, and nothing warns you.

In practice `rc.init` and `rc.shutdown` only ever ran `66 tree start|stop
${TREE}`, so there is nothing to carry over. The two that carried real hooks:

- **`rc.init.container`** held the "append the command to launch inside your
  container here" block and a settable `HALTCODE`. There is **no drop-in
  replacement**: a container is now a fully supervised system, so that command
  has to be re-expressed as a 66 service. See the new
  [container guide](66-container.html).
- **`rc.shutdown.final`** was the documented last-resort hook for actions after
  every filesystem is unmounted. The final stage is now compiled C, so **it has
  no replacement**. The upstream skeleton shipped empty and told admins to leave
  it empty, so most systems are unaffected. But if you used it, there is
  nowhere to put that logic today. Please report your use case.

### Containers

Container mode is now selected **only** by `66 boot -c` / `--container`.
`CONTAINER`, `RCINIT_CONTAINER`, the `CONTAINER_HALTCMD` environment channel and
the `rc.init.container` skeleton are all gone.

A container now follows the same path as bare metal, which is the real change:
**a container is a fully supervised system, not a wrapper around a single
command**. That is why a customized `rc.init.container` has no mechanical
migration.

The exit-code protocol is unchanged in spirit: pid 1 exits `0` by default, `111`
if the boot failed; `66 halt` makes pid 1 exit with the `EXITCODE` found in the
halt file; `66 poweroff` and `66 reboot` report `SIGINT` and `SIGHUP`.

A container built from a bare image now **sets its own state up** instead of
aborting: initialising the version marker is best-effort, and boot skips a tree
resolution it never used. Three container-only bugs are also fixed, each of
which meant a container **never stopped**: the generated final-stage script was
written empty, that script killed itself before it could stop the scanner, and
the boot logger echoed every line back into its own fifo and looped forever.

The new [container guide](66-container.html) covers the whole thing, with build
paths for the Obarun base image, Alpine and Debian. The dockerfiles live in
`contributions/docker/`.

### The `66-shutdown` program is gone

Its job — planned shutdown time, `/etc/66/shutdown.allow`, the wall message,
cancelling — is folded into the `66 halt`, `66 poweroff` and `66 reboot` verbs,
which gain `-c`/`--cancel`. If you called `66-shutdown` directly, call the verb
instead.

`66-shutdownd` moves from `bindir` to `libexecdir` — it is an internal daemon,
not a user command. Anything hardcoding `/usr/bin/66-shutdownd` breaks.

### Other boot fixes

- **An enabled empty tree hung the boot forever.**
- **`CATCHLOG=0` never booted** — the readiness fd was wrong, so the boot never
  got past stage 2.
- **A boot could silently drop `TREE` on some machines and not others** — the
  rebuilt environment was not NUL-terminated, so a key lookup could miss.
- **`66 reboot -f` crashed** — the shutdown verbs no longer route through the
  system sanitation.
- Boot output is no longer interleaved out of order.

---

## What happens by itself

You have nothing to convert by hand, that is the point of this section. But
*automatic* is not *nothing to do*: once the migration has run you reboot, and
that is the whole of your part. The rest is here so you know what 66 is doing on
your behalf, and the one place it can stop.

**The migration is automatic.** 66 detects the version change on the **first
command you run** after the upgrade and converts the ecosystem. A snapshot named
**`system@<your current version>`** (for example `system@0.8.2.2`) is taken
beforehand, so the previous state is always recoverable.

Under the hood, 66 converts every service resolve file to a new internal layout,
rebuilds your logger run scripts to use `66-log` instead of `s6-log` preserving
the backup count, timestamp, max size and destination (the notification flag
takes the `66-log` default), points the scandir at `fdholderd` instead of
`fdholder`, and gives oneshot and module services a scandir entry that the new
scanner will not try to supervise.

Supported source versions are `0.8.0.0` through `0.9.0.0`; anything older is a
hard error, not a silent fallback. From `0.8.2.2` it is a single step.

Three things worth knowing:

- **A reboot is expected.** The migration rewrites your on-disk state, but a
  running `0.8.2.x` scandir keeps its old in-memory picture until you reboot.
- **Each base migrates on its own, as whoever runs `66` first.** Root's
  `/var/lib/66` migrates on root's first command; each user's `~/.66` migrates on
  that user's first command. There is no "migrate everything" command.
- **The scandir conversion is best-effort, and only a genuine disk error
  interrupts it.** If a resolve file cannot be read or written, 66 skips that one
  service and warns rather than aborting the whole migration; the warning tells
  you to force the *next* reboot with `66 reboot -f`, which brings `/run` back up
  clean. On a healthy disk none of this fires. Every record the shutdown needs
  is written, and the reboot is the ordinary one above.

Migrating a `0.8.0.0`–`0.8.1.1` resolve file now also preserves the logger
timeout instead of dropping it, and fixes the logger directory ownership by
honoring the service's own logger `RunAs` instead of forcing the default runner.

---

## New things to explore

Nothing here is required. Your services keep working without touching any of it.

### The event system

A service can now react to something happening elsewhere on the system by
running a `66` command on itself. It is entirely opt-in: a service that declares
nothing behaves exactly as before. Two roles:

- A **reactor** is an ordinary `classic`, `oneshot` or `module` service with an
  `[Event]` section. When its trigger fires, it runs a `66` command on itself
  (`Do`) and/or raises a named event (`Emit`).
- A **source** is a service of the new **`Type = event`**. It runs no process and
  has no `[Start]` section; `66 start` and `66 stop` on a source *arm* and
  *disarm* it.

A reactor triggers on a service's state or result, on a signal, on a filesystem
event, on a cron expression, on a timer, or on a user event raised with the new
`66 emit` command. For instance, restarting a service when its configuration
file changes on disk:

```
[Main]
Type = oneshot
Description = "A simple proof of event concept"

[Start]
Execute = ( echo "I restart myself when /etc/my-daemon is edited" )

[Event]
EventType = inotify
From = ( my-config-watcher )
Do = restart
```

with the source it depends on:

```
[Main]
Type = event
EventType = inotify
Watch = /etc/my-daemon
On = ( IN_CLOSE_WRITE )
```

One `66-eventd` daemon serves the whole scandir; it is created for you at
`66 scandir create` time. A reactor that feeds itself is caught by an anti-loop
backstop: more than 10 fires within a sliding 10-second window disarms it, and a
fresh `66 start` is needed to bring it back.

A `Do = start` reactor of type `oneshot` or `module`, held down until its trigger
fires, reports the new **`waiting`** state.

The full grammar, the vocabulary of each `EventType`, and the runtime behaviour
are on the [event system](66-event.html) page — see also
[66-eventd](66-eventd.html) and [66 emit](66-emit.html), and the `[Event]`
section of the [frontend](66-frontend.html) reference.

### Per-session variables now reach your services

A scandir is started long before a session exists — a user one at the first
login, the system one at boot — and its environment is frozen at that point. A
variable that only a session knows (`DISPLAY`, `WAYLAND_DISPLAY`, `XAUTHORITY`)
could not reach the services it supervises at all. The new
[`66 env`](66-env.html) command is the way in:

```
66 env import DISPLAY XAUTHORITY
```

`import` copies the values **from the environment of the caller**, verbatim.
Nothing is computed, guessed or checked — a variable that is not set is skipped
with a warning. `set`, `unset` and `list` complete the command.

What is published lands in `%%livedir%%/environment/<uid>/`, one file per
variable, and `66-execute` merges that directory into the environment of
**every** service of the scandir at each start, no frontend has to declare
anything. The merge happens **last and verbatim**, so a published value
overrides the `[Environment]` section, the service configuration and the
`ImportFile` files, and a `${...}` inside it stays literal.

Each publication raises `env.<variable>`, each withdrawal `unenv.<variable>`, so
a service can wait for the value it needs rather than be ordered around:

```
[Main]
Type = classic
Description = "Clipit daemon"

[Start]
Execute = ( clipit )

[Event]
EventType = user
On = ( env.DISPLAY )
Do = start
```

The directory is created and destroyed with the scandir, so nothing published
survives a `66 scandir remove` or a reboot. Until you reboot,
the scandir carried over from `0.8.2.x` has no such directory yet, and `66 env`
says so rather than publishing into a store no service reads. At most 20
variables can be published at a time; the twenty-first is refused with an
explicit error.

### New commands

| Command | What it does |
| --- | --- |
| [`66 log`](66-log.html) | read the logs of a service, of the system, or of everything interleaved — with `-f` to follow, `-g` to grep, `-s`/`-u` to bound a time range |
| [`66 emit`](66-emit.html) | raise a user event by name |
| [`66 env`](66-env.html) | publish per-session environment variables to every service of the scandir |
| [`66 runstate`](66-runstate.html) | dump a service's runtime record, the low-level counterpart to `66 state` |
| [`66 fdholder`](66-fdholder.html) | manage the scandir's fd holder daemon |
| [`66 suspend`](66-suspend.html) | suspend the system to RAM |
| [`66 hibernate`](66-hibernate.html) | hibernate the system to disk |

No command was removed.

### Long options everywhere

Until now every 66 program accepted **short options only**. The whole suite now
accepts long options too — `--help`, `--verbosity`, `--live`, `--tree`,
`--timeout`, `--color`, and per-command forms such as `--field`, `--no-name`,
`--follow`, `--graph`. Both `--opt=value` and the glued `-tfoo` form work. Short
options are unchanged, so **your scripts keep working**.

`66 signal` also gains `--stop-group`, `--cont-group` and `--kill-group` to
signal a service's whole process group.

### `66 status` tells you more

Every service type now reports the same way; there is no `s6-svstat` fork any
more, and oneshot, module and event services get the same treatment as classic
ones:

```
Status : enabled, up (pid 731) since 9h 40min by user
```

Durations are now human-readable (`9h 40min`, `40min`, `45s`) instead of a raw
second count, and there is no separate `ready` figure. A notable last result is
shown in parentheses after the pid (`(exited 1)`, `(signaled 9)`, `(crash)`)
and nothing on a clean success, as above.

The new part is **`by <who>`**. who caused the last transition: `user`, `boot`,
`event`, `shutdown`, or `self` when the service acted on its own.
`unsupervised` disappears as a status word, `waiting` appears, and a stopped
service in the graph view (`-g`) now prints `down` instead of a bare `0`.

`66 state` displays 7 lines instead of 8: the `isup` flag is gone, since whether
a service is up is now read from the runtime record.

### Fields are selectable

`66 status` and `66 tree status` gain `-f`/`--field` to choose the fields to
display; **`-o` is deprecated** — it still works and warns. The same
`-f`/`--field` and `-n`/`--no-name` pair is now on `66 state`, `66 resolve`,
`66 tree resolve` and `66 runstate`, which previously took `-h` only.

### Options that no longer exist

| Gone | Use instead |
| --- | --- |
| `66 scandir reload` | `66 scandir reconfigure` |
| `66 scandir create -s\|--skeleton` | nothing — boot reads `init.conf` itself |
| `66 tree start -f` | nothing — `-f`/`--fork` is for `tree stop` and `tree free` |
| `-o rename` on `66 tree create\|admin` | nothing — it parsed but never renamed anything, and now errors instead of doing nothing silently |

`66 tree remove|enable|disable|current` and the `66 scandir` signal subcommands
now accept `-h` only. They used to advertise administrative options they never
applied.

---

## Under the hood

You do not have to do anything about this section. It is here because a few of
these choices surface in places you may touch.

**66 builds against `oblibs` only.** `skalibs`, `s6` and `execline` are gone from
the build entirely. `execline` remains a **runtime** dependency, and only that:
the generated service scripts still chainload it.

The programs 66 used to borrow are now its own: `s6-svscan` → **`66-scandir`**,
`s6-supervise` → **`66-supervise`**, `s6-log` → **`66-log`**, `s6-svc` →
**`66-svctl`**, the `s6-fdholder` suite → **`66-fdholderd`**, `s6-setuidgid` →
**`execl-runas`**.

Oneshot services no longer go through `s6-sudo`. They are handled by
**`66-oneshotd`**, a daemon written for that purpose, not an `s6-sudo`
equivalent. The `66-oneshot` and `66-fdholder-filler` helpers are gone, absorbed
by their daemons.

Where that surfaces for you:

- **Your loggers now run `66-log`.** The migration rebuilds their run scripts.
  `Build = custom` in `[Logger]` remains the escape hatch if you want `s6-log`
  and its full feature set.
- **`execl-runas` is not a drop-in `s6-setuidgid`**: it never introduces
  `UID`/`GID`/`GIDLIST` in the environment, and the `user:group` colon syntax is
  **not** supported — the account is always a single user name.
- **The scandir control directory is renamed** `.s6-svscan` → **`.66-scandir`**.
  Anything of yours reaching into it directly must follow.
- **`66-scandir` is not `s6-svscan`**: it exposes only `-t` and `-d`. The
  per-scandir limits are compile-time options, and `66 scandir create -L
  <log_user>` is unaffected.

The internal **resolve** file is also split into an autonomous core plus addons,
which lets 66 answer most runtime questions without loading everything. This is
invisible to you (the migration rewrites it) but it is a **breaking change for
external users of `lib66`**, such as `66-tools`: `resolve_read_g` / `write_g` /
`check_g` / `remove_g` are renamed to `resolve_read` / `write` / `check` /
`remove`, and the raw primitives take an `_at` suffix.

---

## Bug fixes

Beyond the boot, container and migration fixes above, and all of them reachable
from ordinary use:

- **Parsing any service that has a logger smashed the stack**, the run-script
  buffer was ~18 bytes too small. The same path also used a pointer that a later
  allocation could invalidate.
- **A frontend could crash the parser**: a `[`…`]` run of 20 characters or more
  inside script context (a shell array index, a glob) overflowed a fixed buffer;
  a frontend whose very first byte is a key read far outside the buffer; a
  duplicated section overran the section table (now reported as *too many
  sections in frontend file*).
- **Every key value parsed overflowed its allocation by one byte**, as did the
  fallback `Description` of a service that declares none, and the snapshot name
  built during an upgrade.
- **A service name shorter than 6 bytes** (`svc`, `ntpd`) caused an
  out-of-bounds read at validation, and every non-instanced name read one byte
  before its buffer.
- **`66 enable` / `66 disable` used a dangling tree name**, and **`66 configure
  -V`** read freed memory.
- **`66-shutdownd` wrote out of bounds on every shutdown or reboot** while
  generating its final-stage script.
- **Memory leaks**: `66 tree status` leaked per tree displayed, `66 remove`
  leaked on every removal of a classic service with a logger, a malformed
  frontend leaked per parse error, and `66 -h` leaked.
- **`Nice` was never honored**: a service declaring any `Nice` value ran at
  nice **19**, while a service declaring none correctly ran at 0.
- **`CapsBound` and `CapsAmbient` together killed the service** with exit 111
  and *CAP_SETPCAP is not in the bounding set of the process*. Each key worked
  alone.
- **The container scandir crash handler could not find `66-nuke`** on any install
  where the execline prefix differs from `bindir`.
- A missing `User` key in `[Main]` no longer warns, it is a defaulted value, not
  a problem.
- `66 halt --help` no longer dies with *unable to set the tree name* on a
  tree-less system, and `66 free -P` works again.
- The CLI `-T`/`--timeout` override is now a 64-bit millisecond value; it used to
  be capped at ~49.7 days.

---

## For packagers

- **The only build dependency is `oblibs >= 0.4.0.0`.** `skalibs`, `s6` and
  `execline` are no longer needed to build. `execline >= 2.9.6.1` is still needed
  at **runtime** for the generated scripts.
- **Meson is the only build system.** `configure`, `Makefile`, `package/` and the
  build helper scripts are removed. The old system is no longer deprecated — it
  is gone.
- **Options renamed**: `s6-log-user` → **`66-log-user`**, `s6-log-timestamp` →
  **`66-log-timestamp`**, `s6-log-notification` → **`66-log-notification`**.
- **Option removed**: `sysdeps-dir`. Cross-compilation uses `with-include-dir`
  and `with-dynamiclib-dir`.
- **Options added**: `execline-bindir` and `execline-extbindir` (default
  `/usr/bin`) — the path prefixes for execline binaries in generated scripts, now
  separable from `bindir`; and `doc-only`, which builds the documentation alone,
  skipping the C sources and the `oblibs` lookup.
- **Install layout changed**: `66-shutdownd` moves to `libexecdir`; the new
  `66-scandir`, `66-supervise`, `66-log` and `execl-runas` go to `bindir`;
  `66-eventd`, `66-fdholderd`, `66-oneshotd` and `66-svctl` to `libexecdir`.
  `66-shutdown`, `66-oneshot` and `66-fdholder-filler` no longer exist.
- **Documentation** builds with meson targets instead of configure-time scripts,
  so a documentation failure now fails the build. `lowdown` is still required
  when `with-doc=true`. **mkdocs is not needed** — meson only generates the
  mkdocs sources; the site build runs in CI. `meson compile -C build doc` builds
  the documentation alone.
- The test suite grew from 4 to 36 files and runs under **address and undefined
  sanitizers** in CI, on merge requests as well as master. Enable it with
  `-D test=true` and run `meson test -C builddir`.
- Copyright headers are normalized to a single year per file — relevant if you
  extract copyright years automatically.

## Notes

- New onboarding guides are available: [getting started](66-getting-started.html),
  a [cheatsheet](66-cheatsheet.html), [dependencies](66-dependencies.html),
  [logging](66-logging.html), [troubleshooting](66-troubleshooting.html),
  [coming from systemd, OpenRC or runit](66-migration.html) and
  [running 66 inside a container](66-container.html).
- For detailed documentation on new keys and features, refer to the
  [documentation](https://web.obarun.org/software/66/latest).

---

# In 0.8.2.2

## Overview

This is a bug fix release and convenient build against the latest skarnet software version.

## Behavior Changes

- **Allow to reconfigure an unsupervised service** (e855a0d):

    Previously, reconfiguring a service required it to be supervised. This change allows the `reconfigure` command to work on services that are not currently supervised — only supervised services will be stopped during the reconfiguration process.

## Bug Fixes

- **ChangeDirectory, CapsBound and CapsAmbient sanitization** (497eda3):

    Fixed a bug where the `ChangeDirectory`, `CapsBound` and `CapsAmbient` entries from the `[Execute]` section were not correctly sanitized during the resolve phase, potentially leading to missing or corrupted values.

- **Meson build default option typos** (b096fa1):

    Fixed typos in `meson.build` default options where `s6_log-user`, `s6_log-timestamp` and `s6_log-notification` were using underscores instead of hyphens, causing build failures.

---

# In 0.8.2.1

## Overview

This is an hot fix release and only concern a build make by the Meson build system. Compilation with the previous build system is not affected.

##  Bug Fixes

- **66-shutdownd installation** (d75acc6):

    Fixed the path installation of the `66-shutdownd` program to `/usr/bin` instead of `/usr/libexec`.

---

# In 0.8.2.0

- Adaptation to `oblibs` `0.3.4.0`

## Overview

This document outlines the changes, new features, and bug fixes introduced in the *66* service management system from version `0.8.1.1` to `0.8.2.0`. It is intended for users and administrators upgrading their systems to version `0.8.2.0`. Please read these notes carefully to understand the changes.

### New Features

- **Introduction of the `[Execute]` Section** (30647e8):

   A new `[Execute]` section has been added to configure tasks executed just before calling `exec` for a service’s start and stop processes. This section enhances service configuration flexibility.

   - New Keys in `[Execute]` Section:
     - `ChangeDirectory` (a6f4bfa): Sets the working directory via `chdir()`.
     - `Nice` (de8aefc): Configures CPU scheduling priority via `setpriority()`.
     - `UMask` (b0e24b4): Sets the file creation mask via `umask()`.
     - `BlockPrivileges` (edfa67d): Restricts privileges via `prctl()`.
     - `LimitXXX` (de8bccd): A family of keys (e.g., `LimitNICE`, `LimitNOFILE`, `LimitRTPRIO`) to configure resource limits via `setrlimit()`.
     - `CapsBound` and `CapsAmbient` (9e9fd64): Sets bounding set and ambient capabilities without requiring `libcap` dependencies.

- **New `Conflict` Key in `[Main]` Section** (731f708):

   A new `Conflict` key allows the declaration of conflicting services, improving service dependency management.

- **Non-Mandatory Keys in Service Files** (d3fbcee):

   The `Description`, `User`, and `Version` keys are no longer mandatory.

   Defaults are:

   - `Description`: Set to "<service_name> service".
   - `User`: Set to the name of the process owner.
   - `Version`: Set to the installed version of 66.

- **Tree Creation with Enable Option** (dceeb88):

   Trees can now be enabled at creation time, streamlining tree administration.

- **Meson Build System Adoption** (1f68ff1):

   The project has transitioned to the Meson build system, offering improved cross-platform support and build efficiency. The traditional `configure` and `make` system remains functional during the transition. Users are encouraged to test the Meson system and provide feedback. See [`INSTALL_MESON.md`](https://git.obarun.org/Obarun/66/-/blob/master/INSTALL_MESON.md?ref_type=heads) for details.

### Bug Fixes

- **Logger Destination Ownership** (Critical) (6079cf7):

   Fixed a significant bug introduced in version `0.8.0.0` that incorrectly set the logger destination owner to `root` for `66log` type loggers and removed the `runas` field during migration. This release fix the ownership of the logger destination during the migration phase but only for service logger of type `StdOut=66log`.

- **Service Crash Handling** (c035a94):

   Ensured a fatal error is triggered when a service crashes, improving reliability.

- **Compiler Warnings** (c440c4d, cd9a0c2):

   - Removed GCC/Clang warning messages.
   - Fixed a compiler warning message.

- **Migration Process Fixes** (96d4b49, 6a3aae7, 9e8d6c9):

   - Ignored unknown logger directories during migration.
   - Fixed handling of `Master` resolve file during migration.

- **File Descriptor and Resource Handling** (e028da2, fe7f14e):

   - Fixed reading and writing of `uint64_t` values.
   - Fixed log file display with `status` command for `StdOut=file:/path/to/file` redirection type.

- **Memory Leak Prevention** (7a36e5a):

   Addressed a memory leak issue.

- **Miscellaneous Fixes** (b8a963b, 2d96003, b138e98, 6ab8a36, a04c574):

   - Fixed option name and removed invalid key destination in examples.
   - Fixed headers and reorganized display order fields.
   - Fixed typo and ensured Linux-specific compatibility.
   - Removed useless header file.

### Enhancements

- **CI/CD Enhancements** (1afbec5, 3677099):

   - Switched CI/CD pipeline to use the Meson build system.

- **New Boolean Syntax** (67de347):

   A new boolean syntax has been introduced, documented for the `BlockPrivileges` key.

- **Version Comparison API** (044d4ff):

   Adapted to the new `version_compare` API interface and added tests for the function.

### Migration Notes

- **Migration Process for 0.8.2.0** (9e8d6c9):

   A dedicated migration process has been provided to ensure a smooth transition to version `0.8.2.0`. Please, consider the introduced bug below. The migration try its best to fix this issue by itself but it may impact your migration process as the migration process only handle with service logger of type `StdOut=66log`. If after a restart of a service logger you get a down process, ensure that your loggers directories have the correct permissions. The error should be visible at the `/run/66/log/0/current` uncaught-logs file.

- **Logger Destination Ownership** (6079cf7):

   If you are upgrading from version `0.8.0.0` or `0.8.1.1`, verify the ownership of logger destinations for `66log` type loggers, as the bug fix may affect existing configurations and upgrade process.

- **Meson Build System** (1f68ff1):

   Users are encouraged to switch to the Meson build system and to consider the old build system as deprecated. The old `configure` and `make` system remains supported during the transition. Refer to `INSTALL_MESON.md` for setup instructions.

## Notes

- For detailed documentation on new keys and features, refer to the [documentation](https://web.obarun.org/software/66/latest).

---

# In 0.8.1.1

## Bug Fixes

- Adapt correctly `reconfigure` command to the new graph API and respect `-P` option.

---

# In 0.8.1.0

- Adaptation to `oblibs` 0.3.3.0


## Overview

This release of the 66 project includes several bug fixes, new features, and improvements to enhance the functionality and maintainability of the service management system. Below is a summary of the key changes introduced in this release.

## New Features

- Service Alias Support (a36a4fad): Added a new `Provide` key to allow defining aliases for service names, improving service management flexibility.
- Environment File Import (4426ab70): Introduced a new `ImporFile` field in the [Environment] section to support importing environment files.
- Simplified Execute Field Writing (099872e0): Enhanced the `Execute` field to permit spaces, tabs, carriage returns, and newlines (removed during processing) for custom builds, improving readability and usability.
- Sanitize Resolve Function (c91ff167): Added a new sanitize_resolve function to ensure the validity of resolve files for trees and services for the targeted version.
- Tree Graph Build System (afb18e68): Implemented a new `tree_graph_build_system` function to improve system graph generation.
- Split Info Walk Functions (a058225c): Separated info_walk into `service_info_walk` and `tree_info_walk` for better modularity and clarity.
- Enum and Macro Enhancements (ddc04da2): Split enum definitions and introduced macros to automate enum, string table, and struct generation. This reduces error-prone code, improves readability, and simplifies maintenance. Added an enum test suite.

## Bug Fixes

- Dependency Cleanup (e2b2c539): Fixed unnecessary entries from the dependency field of the requiredby dependency for the service to remove.
- Empty Field Values (b4be03f2): Fixed handling of empty values for fields like contents, dependencies, requiredby, optional depends, and regex fields for directories, files, and infiles, allowing modules to set empty strings (e.g., when services are commented out).
- Frontend Field in Modules (a9e7ce23): Corrected the setting of the frontend field for services inside modules.
- Frontend Dependencies Preservation (7bc3cab2): Ensured that dependencies declared in the `frontend` file for a module are always retained.
- Tree Name Respect (2b306df2): Fixed an issue where the previous tree name was not respected for services in a selection when the `-t` option was not used.
- Scandir Symlink (df68b262): Added support to remake the service scandir symlink, ensuring its presence.

## Improvements

- Migration Support:

    * Continued support for `0.7.2.1` version migration (1cb94c73).
    * Provided migration processes for version `0.8.1.0` (d2288e73) and `hiercopy` field changes (958ee60c).
    * Ensured snapshots are created in all migration cases (0270ebda).

- Code Maintenance:

    * Renamed `hiercopy` to `copyfrom` internally (3411e29f) for consistency.
    * Adapted to a new enum interface (cf8359c2) and graph algorithm (33287ebd) with new structures, functions, and macros.
    * Made the `set_info` function public (958a0964) for broader usability.
    * Removed redundant `graph_remove_deps` function (10c83ba7) and moved hash functionality to oblibs (7c491133).
    * Cleaned up header files (a83db505) and removed unnecessary parameters (8ea7f830) for cleaner code.


- Documentation:

    * Fixed typos and improved documentation (5f5c8ee9, 89ef88d8).
    * Updated the project roadmap (fc97c4e3) to reflect current plans.

- Miscellaneous

    * Minor Changes (4767cdc4, 89ef88d8): Addressed typos and minor code adjustments with no significant user-facing impact.

## Notes

- This release ensures that users upgrading from previous versions will have an operational system without manual intervention. Migration processes for `hiercopy` internal field changes and version `0.8.1.0` are handled automatically.

- For detailed information on new fields and functions, refer to the updated documentation.

---

# In 0.8.0.2

- Adaptation to `oblibs` 0.3.2.1

## Bug Fixes

- Different documentation typo and bugs fix.
- Restart command: explicitly call stop and start command separately to avoid notification readiness issues. Retrieve the exact same services states for interdependences services after a restart command.
- Frontend parsing: handle all possible cases on '()' format.
- reconfigure command: respect `-p` option.
- 66-execute: respect exclamation mark to avoid polluting the environment variable.
- status command: avoid to crash reading status of service just after using a `66 remove -P` command.
- scandir command: fix stack overflow at `create` command.

---

# In 0.8.0.1

## Bug Fixes

- Always update state of `fdholder` and `oneshotd` daemon even in case of crash.
- Avoid badly formatted environment variables coming from EFI.
- Clean the algorithm of the shutdown procedure to avoid killing `fdholder` and `oneshotd` before the very end of the process.
- Fix `connection refused` error at fdholder connection.
- Prevent extra characters during the parse process of the `init.conf`.

---

# In 0.8.0.0

- Adaptation to `oblibs` 0.3.1.0

## Synthesized Release Notes

- Nearing the End of Breaking Changes: This release marks a major step towards the end of planned breaking changes for version 66. Version `0.7.0.0` introduced stability in the directory hierarchy of the 66 ecosystem, and version `0.8.0.0` solidified the stability of frontend file keys. While the implementation of events may happen in version `0.9.0.0`, this can potentially occur without breaking existing 66 components(see [roadmap](https://git.obarun.org/Obarun/66/-/blob/master/README.md#roadmap)).

- Introduction of Snapshot Command: A new [snapshot](66-snapshot.html) command has been added, offering a comprehensive backup solution for the 66 ecosystem. This feature guarantees that you can restore the exact state of the ecosystem before any changes. It also enables the replication of ecosystems across different hosts to maintain consistent states and behaviors. Importantly, using snapshots **does not alter the state of running services**.

- Transparent Upgrade Process: The release includes a new [upgrade process](66-upgrade-process.html) that is fully transparent to the user. Whether a migration involves changes to the resolve file or other components, 66 automatically detects version upgrades and makes the necessary adjustments, ensuring continuity with the new version. This process **does not affect the state of running services**, and an automatic snapshot is created during the migration to facilitate easy recovery if needed.

## New features

- New keys for [Input/Output redirection](66-standard-io-redirection.html) (`StdIn`, `StdOut`, `StdErr`) have been added to control the behavior of standard input, output, and error, respectively.
- A new [`66 snapshot`](66-snapshot.html) command allows users to create, remove, list, and restore snapshots, providing enhanced backup and reliability management.
- An [automatic migration process](66-upgrade-process.html) is now available, ensuring seamless upgrades. This process is completely transparent to the user and is triggered by any 66 command following an upgrade of the 66 program.

## Frontend files

- Key Name Formatting Changes: The `@` prefix in key names has been removed, and key names now start with a capital letter. Additionally, the `-` character has been removed from composite names and also replaced by a capital letter. 66 will automatically migrate the frontend files present on your system, but this will only apply to active services(i.e., those listed when running `66 tree status` with the field `contents`). You may need to manually translate your own frontend files.

To help with this, use the script below by running:
`./migration_service.sh /path/to/my/<FrontendFile>`

```
#!/bin/sh

service="${1}"

sed -e "s:\[main\]:\[Main\]:g" \
    -e "s:@type:Type:g" \
    -e "s:@description:Description:g" \
    -e "s:@version:Version:g" \
    -e "s:@depends:Depends:g" \
    -e "s:@requiredby:RequiredBy:g" \
    -e "s:@optsdepends:OptsDepends:g" \
    -e "s:@options:Options:g" \
    -e "s:@flags:Flags:g" \
    -e "s:@notify:Notify:g" \
    -e "s:@user:User:g" \
    -e "s:@timeout-kill:TimeoutStart:g" \
    -e "s:@timeout-up:TimeoutStart:g" \
    -e "s:@timeout-finish:TimeoutStop:g" \
    -e "s:@timeout-down:TimeoutStop:g" \
    -e "s:@maxdeath:MaxDeath:g" \
    -e "s:@down-signal:DownSignal:g" \
    -e "s:@hiercopy:CopyFrom:g" \
    -e "s:@intree:InTree:g" \
    -e "s:\[start\]:\[Start\]:g" \
    -e "s:@build:Build:g" \
    -e "s:@runas:RunAs:g" \
    -e "s:@execute:Execute:g" \
    -e "s:\[stop\]:\[Stop\]:g" \
    -e "s:\[logger\]:\[Logger\]:g" \
    -e "s:@destination:Destination:g" \
    -e "s:@backup:Backup:g" \
    -e "s:@maxsize:MaxSize:g" \
    -e "s:@timestamp:Timestamp:g" \
    -e "s:\[environment\]:\[Environment\]:g" \
    -e "s:\[regex\]:\[Regex\]:g" \
    -e "s:@configure:Configure:g" \
    -e "s:@directories:Directories:g" \
    -e "s:@files:Files:g" \
    -e "s:@infiles:InFiles:g" \
    -i ${service}
```

See [Rosetta Stone](66-rosetta.html#keyword-table-convertion) for the list of keyword name changes.

### Behavior enhancements

- The [`66 status`](66-status.html) command now provides an overview of all system services, organized by tree, when no specific service is specified.
- The [Identifier](66-identifier.html) feature has been expanded: in addition to `@I`, 66 now recognizes seven new identifiers to facilitate the creation of more generic frontend files.
- Module configuration: Two more news variables are exported at execution time of the `configure` script called `MOD_ENVIRONMENT_ADMDIR=%%environment_adm%%` and `MOD_ENVIRONMENT_USERDIR=%%environment_user%%`.
- The [`66 remove`](66-remove.html) command is designed to always succeed, ensuring that no service becomes impossible to remove.

### Deprecated and Obsolete keywords

The deprecated key `@shebang` has been completely removed and is no longer recognized by the parser.

The `Destination` key (previously `@destination`) in the `[Logger]` section is now deprecated and replaced by new `StdIn`, `StdOut`, and `StdErr` keywords. For compatibility, the parser will automatically handle the conversion.

Removal of Deprecated Options:

- For the `disable` command: `-F`, `-R` options are removed.
- For the `parse` command: `-F`, -C`, `-c`, `-m` options are removed.
- For the `stop` command: `-X`, `-K` options are removed.

## Bug Fixes

- Command line-defined timeouts now take precedence.
- Do not create finish script if section [Stop] doesn't exist.
- Proper handling of errno during signal reception.
- Fixed the resolution of the source frontend file in a module's resolve file for a service.
- Prevented crashes when encountering an empty field in a seed file.
- Corrected parsing errors when an unknown key is found at the end of the frontend file.
- The `66 tree status` command now only displays services associated with a specific tree.
- Fixed behavior of the `-l` option at execl-envfile program.
- Always deal with logger at disable time.

---

# In 0.7.2.1

- Bugs fix:

    - Respect the VERBOSITY during the whole boot process.
    - Fix importation of the kernel command line environment variables.
    - Fix order of precedence for the build of the environment variable used during the whole boot process.

---

# In 0.7.2.0

- Adapt to `oblibs` 0.3.0.1`

- Bugs fix:

    - Logger destination directory for a oneshot or logger destination directory for a service inside a module are now properly deleted at `remove` subcommand invocation.
    - Allow empty modules.
    - Respect the associated tree of a service at `enable` process time if it were already enabled.

- Behavior changes:

    - The `-s` and `-c` options to the `configure` subcommand are now mutually exclusive.
    - Service inside module cannot be reconfigured alone. The whole module must be reconfigured instead.
    - To be consistent between subcommand, the `scandir reload` subcommand was renamed to `scandir reconfigure`. The old subcommand invocation still work.
    - The configuration directory used to build a module is not kept anymore after a parse process. A temporary is used instead.

- New features:

    - The contains of the %%environment_adm%% or %%environment_user%% for root and regular user respectively is imported by default at `scandir start` command invocation.
    Although this can be changed at compile time by passing the `--with-sysadmin-environment=DIR `, `--with-user-environment=DIR` for root and regular user respectively

- Enhancement:

    - Avoid to crash at `unsupervise` process with a corrupted service. This allows to avoid to be stuck at `remove` command invocation for a corrupted service or a corrupted state service within module.
    - Avoid to crash at `stop` process if the service is already unsupervised.
    - Earn rapidity at `unsupervise` process by sending only one signal for all services to the `fdholder` service.
    - Use temporary directory for the creation of a the module configuration directories.
    - Complete review of the *struct enum* ecosystem and passing from an O(n*m) algorithm to O(1). This also serves as preparation for future releases that will include new sections and key fields.

---

# In 0.7.1.1

- Bugs fix:

    - corrected environment computation and string cleanup from the frontend key.
    - fixed the value of the intree key.
    - ensured sufficient space for renaming interdependencies.
    - fixed the use of parse_list functions, even in the case of multiple lines.

---

# In 0.7.1.0

- Adapt to `oblibs` 0.3.0.0

- Bugs fix:

    - make exclusive `-s` and `-c` options at *configure* command.
    - respect `-s` option for edition at *configure* command.
    - provide *wall* command documentation. This command was added from 0.7.0.0 tag but the documentation wasn't.
    - fix closing string at parse time.
    - typo fix.

- Complete review of the *execl-envfile* program. Adapting it to the new lexer and environ familly functions make it faster and easier to debug and use less HEAP memory.

- Remove deprecated *env* key of the `@options` field.

---

# In 0.7.0.2

- Hot fix:

    - Always build the graph with the service associated to a module.

---

# In 0.7.0.1

- Bug fix:

    - Fix buffer overflow at tree administration
    - Typo fix

---

# In 0.7.0.0

- Adaptation to `skalibs` 2.14.1.0
- Adaptation to `execline` 2.9.4.0
- Adaptation to `s6` 2.12.0.3
- Adaptation to `oblibs` 0.2.1.0

This release marks a significant rewrite of `66`, introducing a new UI and serving as a comprehensive service supervision suite and independent service manager.

Primarily, expect no compatibility with previous versions due to:

- The removal of `s6-rc` support. `66` is now a complete independent service management based on s6 for init and service supervision.
- A complete overhaul of the folder structure for storage and runtime directories, simplifying it considerably.
- An overhaul of tree behavior. Trees now function as services, and a complete tree dependency graph has been implemented.
- Services can now depend on each other regardless of whether the service is declared on the same tree or the declaration order of the tree. For instance, if service `Sb` depends on service `Sa` and `Sa` is within `TreeB` while service `Sb` is within `TreeA`, and `TreeB` depends on `TreeA`, launching `TreeA` will start `Sa` even if `TreeB` isn't started first. When `TreeB` is executed, `Sb` will find `Sa` already started and commence directly.

For UI changes, frontend file convertion and clean of the `66` architecture, a [Rosetta stone](66-rosetta.html) is available.

## Frontend Files

### New fields have been added:

- `@requiredby`: Specifies which service is required-by another service. For example, if service Sa declares service Sb as required-by Sa's dependency, Sb won't start until Sa does. This allows building complex graph structures without modifying every frontend file of each service.
- `@earlier`: Declares any service as an earlier one, starting as soon as the scandir is running, similar to tty12. This field is mandatory for services intended to start earlier.
- `@intree`: Specifies the tree to use at enable/start time.

The following field has been removed:

- `@extdepends`: No longer necessary as services can depend on any service regardless of the tree used.

Frontend files for regular account **must be** now localized at `%%service_system%%/user`, `%%service_adm%%/user` or `${HOME}/%%service_user%%`.

### Behavioral changes:

- `@options`:
    - `pipeline`: This option was removed. It was only present for `s6-rc`.
    - `env`: This option was removed. The simple declaration of the [environment] section is sufficient to activate the options.

- `@shebang`: Deprecated but kept for compatibility reasons. Declare your shebang directly within the `@execute` field. Refers to [frontend](66-frontend.html#appendix-b-the-execute-key-in-depth) documentation for futhers information.

- `@build`: Not mandatory anymore, as it will be declared 'auto' by default.

- `@addservices`: This options was removed from the `[regex]` section.

The `classic` type now accepts the fields `@depends` and `@requiredby`. The `classic` type replaces the `longrun` type.

Logger destinations for `oneshot` type services can now be declared on a **tmpfs** directory, particularly useful during boot time.

Services can be started without being enabled first. In this case, the service won't start on the next reboot.

The `bundle` and `longrun` types have been removed, replaced by `classic`, `oneshot`, and `module` types. For compatibility reasons, if your old `frontend` file declares the service as a `longrun` type, the parser will convert it to a `classic` type automatically. No automatic conversion is made for services of type `bundle`.

## [Environment] Section

This section now allows reusing the same variable from the actual environment. For instance:

```
socket_name=!/run/dbus/system_bus_socket
cmd_args=!--system --address=unix:path=${socket_name}
PATH=/usr/local/bin:${PATH}
```

The order of key-value pair declaration **doesn't matter**:

```
cmd_args=!--system --address=unix:path=${socket_name}
socket_name=!/run/dbus/system_bus_socket
```

Variable name **must be** between `${}` to get it value. For instance, `$var` is not replaced by its value.

Double-quote within *value* **must be** escaped with backslash `\`. Refers to the updated documentation of [execl-envfile](execl-envfile.html) for futhers information.

## Module Changes

The `module` directory structure has been completely redesigned for better intuitiveness and comprehensiveness. Expect no compatibility with the previous version; a rewrite is required if you use `module` on your system.

A `module` cannot contain another `module`; instead, you can declare it as a dependency via `@depends` or `@requiredby`. These can also be specified through the `configure` `module` script.

Refer to the specific [module](66-module-creation.html) page for furthers information.

## Trees

Trees now react as services regarding graph dependencies. You can declare a `tree` depending on or required-by others.

A default named `global` `tree` is provided. Services without their localization defined or users not specifying a `tree` to use will be defined within that `tree`.

A `seed` file can be provided for automatic `tree` configuration at creation time. For example, it defines `depends/requiredby` dependencies of the `tree`.

If a service declares a non-existing `tree`, the `tree` will be created automatically with a default configuration, but without any `depends/requiredby` dependencies. To configure the `tree` with specific requirements at creation time, provide and install `seed` file with your service.

## Configure Script

Removed flags:
- `--with-system-module=DIR`
- `--with-sysadmin-module=DIR`
- `--with-user-module=DIR`

Added flags:
- `--with-default-tree-name=NAME`
- `--max-path-size=BYTES`
- `--max-service-size=BYTES`
- `--max-tree-name-size=BYTES`
- `--with-system-seed=DIR`
- `--with-sysadmin-seed=DIR`
- `--with-user-seed=DIR`

The slashpackage convention was removed.

## Skeleton files

The skeleton files `shudown`, `reboot`, `poweroff` and `halt` are removed and replaced by `66 reboot`, `66 poweroff`, `66 halt` command respectively.

## Code Changes

The code has been largely rewritten and simplified, offering more features with approximately the same number of code lines. Additionally, the code now uses less `HEAP` memory, although this optimization is ongoing.

The parser was completely rewritten and heavily optimized, significantly reducing the time to parse a service(by three times).

The start process was rewritten due to the removal of `s6-rc`. `Oblibs` now provide general functions to build any *Acyclic graph*.

The code for the module part was revamped and greatly simplified.

---

# In 0.6.2.0

- **WARNING**: `66-update` is no longer compatible with 66 version under 0.5.0.0.

- Adapt to skalibs 2.11.0.0

- Adapt to execline 2.8.1.0

- Adapt to s6 2.11.0.0

- Adapt to s6-rc 0.5.2.3

- Adapt to oblibs 0.1.4.0

---

# In 0.6.1.3

- Adapt to skalibs 2.10.0.3.

- Adapt to execline 2.8.0.1.

- Adapt to s6 2.10.0.3.

- Adapt to s6-rc 0.5.2.2.

- Bugs Fix:
    - 66-boot: fix call of 66-scandir -c option.
    - configure script: fix installation of skel/init.conf file.

- Configure script: remove the slashpackage convention.

---

# In 0.6.1.2

- Bugs fix:
    - Avoid segmentation fault if 66-init is used without arguments. Thanks Timothy Murphy.
    - Fix the build of man pages.
    - Documentation fix.

- 66-env:
    - Bug fix:
        - Create the user configuration file at **-r** option if it doesn't exist yet.
        - Fix **-c** option.

- 66-inservice
    - Do not output warn message from upstream configuration file at contents display.
    - Bug fix:
        - Respect the number passed by **-p** options.

---

# In 0.6.1.1

- Bugs fix:
    - remove the exclamation mark from variable at module parse time before passing it to the script configure

---

# In 0.6.1.0

- Bugs fix:
    - Avoid to crash if no tree is enabled yet.
    - Documentation fix.
    - Handle correctly the return value of s6_rc_servicedir_manage.
    - Fix `66-all` command at s6-svscan control file of a regular user scandir creation.
    - Handle correctly a same(or commented) `key=value` pair on multiple service configuration file.

- *execl-envfile*:
    - new option:
        - **-v**: allows verbosity changes.
    - Bug fix:
        - Handle correctly a same(or commented) `key=value` pair on multiple service configuration file.

- *66-env*:
    - new option:
        - **-e** *editor*: Allows to choose the editor to use for the edition of the configuration file.

---

# In 0.6.0.1

- Bugs fix:
    - Respect variables value from kernel command line.
    - Fix release number at 66-upgrade.html file.
    - Fix string lenght at regex replacement time.

---

# In 0.6.0.0

- Adapt to skalibs 2.10.0.0

- Adapt to execline 2.7.0.0

- Adapt to s6 2.10.0.0

- Adapt to s6-rc 0.5.2.1

- Adapt to oblibs 0.1.2.0

- Bugs fix:
    - 66-tee -c: check if backup is empty.
    - parse_module: avoid infinite loop at dependencies resolution on sub-module.
    - 66-disable: do not crash if the service is not enabled at state check.
    - execl-envfile: fix parse process for a configuration file.

- *66-disable*:
    - new options:
        - **-R**: removes configuration files and logger directory of the service. So, no components of the service is kept.

- *66-env*:
    - **-r** can be passed multiple time

- *66-tree*:
    - **-U** option is now deprecated and passed to *66-all*.

- *66-all*:
    - new argument:
        - *unsupervise*: unsupervise all services of a tree. This argument replace the `66-tree -U` command.

- *66-inservice*:
    - It displays the absolute path of the file at `Environment file` field.
    - The **-c** option is now no longer available.

- *66-update*:
    - the **-c** option is now no longer available.

- *66-env*:
    - the **-d** option is now no longer available.

- *66-enable*:
    - the -c|m|C|i is now deprecated. The configuration file is handled automatically (see [Service configuration file](66-service-configuration-file.html) for further information.)
    - **-I**: new options to avoid the copy of a modified configuration file.

- *66-parser*:
    - deprecated the -c|m|C to follow changes about the *66-enable* tool.

- *66-scandir*: the ***interface change***.
    - The *owner* argument is now an option(**-o**) and the arguments are *create|remove*.
    - The *up* option is passed to *66-scanctl*.
    - The **-e** option is passed to *66-scanctl*.
    - New options:
        - **-B**: specifies to create a scandir for a container. In this case, a `/run/66/scandir/container/halt` file is created(see [66-boot](66-boot.html) for further information).
        - **-c**: do not set the `catch-all` logger.

- *skeleton file*: (see [66-boot](66-boot.html) for further information).
    - *init.conf*:
        - New variables:
            - *CONTAINER*: specifies to boot inside a container.
            - *RCINIT_CONTAINER*: absolute path to the init file used to boot inside a container.
            - *CATCHLOG*: create or not the `catch-all` logger.
        - Variable removed:
            - *ISHELL*.
    - *rc.init.container*: file used in case of boot inside a container.
    - *ishell*: not longer available.

- *66-scanctl*: the ***interface change***.
    - arguments signal are now: *start|stop|reload|nuke|zombies*.
    - New options: this two options are only valid for a *start* signal.
        - **-d**: file descriptor to use for readiness notification.
        - **-t**: perform a scan every rescan milliseconds.
- *66-boot*:
    - New option:
        - **-z**: to use color

- *66-shutdownd*:
    - swicth **-B** and **-C** (which is renamed to -c) to be consistent with the *66-scandir* tool.

- frontend:
    - **nosetsid** value at `@flags` key is deprecated and have no effects if it defined.
    - new format for the `@options` key:
        - **log**: the logger is automatically created even if the value **log** is not set. If you don't want a logger at all prefix the value *log* with an exclamation mark as it: `!log`.
        - **env**: if the environment section is defined, the **env** value is not mandatory. Also, the old behavior is always valid: if the **env** value is set, the `[envrionment]` must be set.

- html documentation: documentation is now versionned.

- New tool:
    - *66-nuke*: this tool is a strict copy of the [s6-linux-init-nuke] (https://skarnet.org/software/s6-linux-init/s6-linux-init-nuke.html) tool.

---

# In 0.5.1.0

- Bugs fix:
    - Avoid to parse twice a service coming from a service of type module.

- *66-disable*:
    new options:
        - **-F**: forces the *service* to be disabled even if it's already marked disabled. See the `66-disable` documentation page for further information.

---

# In 0.5.0.1

- Bugs fix:
    - Handle old service format where the version directory doesn't exist. This is last release handling this case.
    - *66-tree*:
        Remove the live tree state directory in any case even if the directory is empty.

---

# In 0.5.0.0

This is a ***Major release***, you need to update your *trees* with *66-update* tool. If you skip from a version earlier than 0.4.0.1, the *66-update* will not work. In this case, you need to reconstruct your trees manually.
Downgrading to a previous version will not work either, due to the new format of the *resolve* inner files.

- Adapt to oblibs v0.1.0.0

- Pass the writing of the *resolve* files to a `CDB` format. From that point onward the `66-update` will no longer be mandatory, even after major version release.

- Adapt `66-update` to the new `CDB` format where applicable.

- Bugs fix:
    - Write the dependencies of the `contents` file for a module in the proper order as to avoid multiple repeated names.
    - *66-update*:
        - Get the correct exit status at tree contents process.
        - Fix segmentation fault when a crash occur at enable time.
    - Compilation `configure` script improvements and bugs fix.
    - Fix the location of the modules directory service at enable time. The place of the module frontend file determines the place of the result process for the service module directory.
    - Fix the creation of the logger directory when field `@build` is not set.
    - Fix **-r** signal and **-R** signal behavior at *66-start* tool.

- The `rc.init` skeleton file does not launch the `ISHELL` script anymore during a crash at stage2. It's the responsability of the sysadmin to deal with this error at his convenience.

- `@destination` field in section `[logger]` is no longer mandatory at the use of `@build` with value `custom`.

- *66-env*:
    - **-e** is now the default option.
    - **-L** displays now all `key=value` pair from all files found at the configuration directory.
    new options:
        - **-c**: changes the current symlink to the specified version.
        - **-V**: displays available version.
        - **-i**: import an extra configuration file from one version to another.
        - **-s**: handle a specific version for command **-L|V|e|r**.

- All `key=value` pairs from `init.conf` skeleton file can be now passed to the kernel command line. Also, variables from `init.conf` are now passed to the `rc.init` skeleton file as arguments.

- `@version` field is now **mandatory**.

- The version symlink of the configuration file points now to the configuration directory instead of the configuration file. This allows overwriting a same `key=value` with the writing of an extra configuration file instead of changing the upstream file.

- *66-in{resolve,state}*: field `Real logger name` name is renamed `Real_logger_name`.

- *66-enable*:
    - Allow the use of a different version of a configuration file than the frontend service file, if any of -c/m/C options are used.
    new option:
        - **-i**: import extra configuration files from a previous version.

---

# In 0.4.0.1

- Hot fix: `@build` is no longer mandatory even for `[stop]` section.

---

# In 0.4.0.0

This is a ***Major release***, you need to update your *trees* with *66-update* tool.

- new extra-tools:
    - execl-envfile: this tool come from [66-tools](https://framagit.org/obarun/66-tools.git) software and was incorporated inside the 66 software.
        - It parses now all files found at a directory by alphabetical order.

- Documentation is now written in markdown format. Lowdown software it necessary to build in html and manpages format.

- Bugs fix:

    - *66-update*: respect the tree start order.
    - *66-tree*: fix behavior when we have only one tree enabled.
    - *66-inservice*: do not crash in case of empty log file.
    - Fix reverse dependencies search for a service with type `module`.
    - Respect timestamp format given at compile time for the uncaught-logs.
    - Accept empty environment file.

- Configuration service files: These files is now automatically versioned in function of the field `@version` declared on the frontend service file e.g. `/etc/66/conf/<service>/version/<file>`. A symlink called `version` is created or updated at `/etc/66/conf/<service>/version`. This symlink point to the configuration file currently in use.
Also, you can now write and save your own configuration file for a service inside the configuration service directory. All files found on that directory will be parsed and exported to the environment of the service at start time. The parse process is made on alphabetical order.

- `@hiercopy` field accept now relative path.

- `@build` field is no longer mandatory. If it not set, `auto` is the default.

- `@version` field **must** be in the form digit.digit.digit e.g. 0.10.2.

- `module` service type:

    - General code improvement and evolve.
    - Add `@addservices` field at `[regex]` section.
    - Disabling a service contained inside a module is not possible. This break entirely the module operation.
    - Sub module directories is no longer mandatory. *66-enable* will create it if it doesn't exist.
    - *66-enable* export some variable environment at configure script runtime-see modules-service documentation page.
    - Fix cyclic dependency: a module cannot call it itself.
    - Redesign of the inner directories structure: instantiated service service **must** be define at `service@/` sub-directory where other type go to `service/` sub-directory. `.configure/` is now named `configure`.
    - All configuration files for each service contained inside the module is written inside the versioned directory of the module e.g. `/etc/66/conf/<module>/<version>/<service>/<service_version>/file`. This allows to have multiple module using a same service with a specific configuration for each service.

- *66-inresolve*: add field Real_exec_run,Real_exec_finish for the service and logger associated to display the exact contain of the `run/up` and `finish/down` files respectively.

- *66-enable*: The absolute path of the frontend service can also be set but **must** contain the primary path of the path define at compile time by --with-system-service or --with-sysadm-service or --with-user-service e.g. `/etc/66/service/lamp/httpd`.

- *66-env*:
    - General code improvement.
    - Follow the change about the versioned configuration service file.

---

# In 0.3.0.3

- Hot fix: Avoids overwriting the current file

---

# In 0.3.0.2

- Fix check and installation of user configuration file directory.

- Add missing *66-inresovle* and 66-instate* documentation.

---

# In 0.3.0.1

- Fix `@optsdepends` and `@extdepends` behavior:
    These two fields now respect correctly the start order of the service's dependencies.

- *66-inservice* change:
    The field `Optional dependencies` and `External dependencies` displays now the name of the tree where the service is currently enabled after the `:` colon mark if any:

    ````
    External dependencies : dbus-session@obarun:base gvfsd:desktop
    Optional dependencies : picom@obarun:desktop
    ````

---

# In 0.3.0.0

This is a ***Major release***, you need to update your *trees* with *66-update* tool.

- Bugs fix

- [skalibs](https://skarnet.org/software/skalibs) dependency bumped to **2.9.2.1**

- [exeline](https://skarnet.org/software/execline) dependency bumped to **2.6.0.1**

- [s6](https://skarnet.org/software/s6) dependency bumped to **2.9.1.0**

- [s6-rc](https://skarnet.org/software/s6-rc) dependency bumped to **0.5.1.2**

- [oblibs](https://framagit.org/oblibs) dependency bumped to **0.0.6.0**

- Add **-z** to all tools to enable colorization:

    - *66-in{service,tree}* and *66-update* tools change the *-c* option to *-z* option to be consistent between all tools.


- *66-start* and *66-stop* exit 0 instead of 110 if the service is not enabled.

- *Frontend* service file change:

    - The field `@type` accepts a new kind of service called *module*. A *module* can be considered as an instantiated service. It works the same way as a service *frontend* file but allows to configure a set of different kind of services before executing the enable process. Also, the set of the services can be configured with the conjunction of a script called `configure` which can be written on any language.

    - *module* also comes with a new section named `[regex]` which contains the following field:
        - `@configure`
        - `@directories`
        - `@files`
        - `@infiles`

        Please see the *frontend* documentation for futher information.

    -  Allow commenting out of a service inside *@contents*, *@depends*, *@optdepends*, *@extdepends* field with the `#` character. The service name **must** begin with a `#` character without any space between the `#` and the name of the service, so it can be ignored.

        ````
        @depends = ( foo #bar fooB )
        ````

    - Add `@version` field:
        This field is currently not mandatory to allow time to adapt the existing service files on your system, but it **will be mandatory** in a future release.

    - Comments in the `[environment]` section is now kept at parsing process and written to the final service configuration file. This is useful to explain the use of a variable without the need to look on the executable script.

- *66-in{service,tree}* display now *up*,*down* or *unintialized* on status and graph dependencies field for *oneshot*, *bundle*, *module* services. This allows us to know if e.g an *oneshot* service is currently *up* or *down*.

- *66-enable* now accepts the new option **-m**. This option reacts the same as the **-c** option for new *key=value* pair but overwrites the change of the admin on existing *key=value* pair. A *key=value* pair which doesn't exist on the frontend file remains untouched.

- *Oneshot* now accepts the option *log* at the field `@options`. This allows a *oneshot* to have its own logger. As any other service the log file can be seen with the *66-inservice* tool. The log destination can be controlled by the `@destination` field at the `[logger]` section. Default is set at compile time by the *--with-system-log* or *--with-user-log* flag.

- New debug tools:
    - *66-inresolve*: This tool allows to read the contents of the `resolve` file.
    - *66-instate*: This tool allows to read the contents of the `state` file.

- *--disable-s6-log-notification* was added on the *configure* script to disable logger's [readiness notification](https://skarnet.org/software/s6/notifywhenup.html). By default it use the file descriptor number 3.

- *--with-s6-log-timestamp* flag was added on the *configure* script to set the default output date format for a logger at the compile time. See `configure --help` command.

- *--with-system-module*, *--with-sysadmin-module*, *--with-user-module* flags was added on the *configure* script to set the default *system*, *sysadmin* and *user* module directory respectively.

- *--with-system-script*, *--with-user-script* flags was added on the *configure* script to set the default *system* and *user* script directory respectively.

----

# In 0.2.5.2


- Fix build: Remove bytes.h oblibs header file


----

# In 0.2.5.1

- Bugs fix

- 66-tree -S options: if after_tree and tree have the same name, tree is considered as the very first tree to start.

- 66-unmountall: do not umount SS_LIVE

- Add SIGPWR control file at creation of .s6-svscan directory

-----

# In 0.2.5.0


- Bugs Fix: Bad memory allocation

- 66-tree: add -S option which allows to order the start tree process even after it creation.

----

# In 0.2.4.1

- Hot fix: fix html documentation

---

# In 0.2.4.0

- Bugs fix: Bad memory allocation.

- Enable again RB_DISABLE_CAD but don't crash if its not available and warn user

    - Add @optsdepends and @extdepends field at [main] section:

        - @optsdepends can be considere as: "enable one on this service or none"

        - @extdepends can be considere as: "enable the service if it is not already declared on a tree"

    - 66-in{tree,service}:

        - in case of empty value the tools return None

        - add -n option: this avoids to display the name of the field.

        - add field at 66-intree:

            - start: displays the list of tree(s) started before

            - allowed: displays a list of allowed user to use the tree

            - symlinks: displays the target of tree's symlinks

        - add field at 66-inservice:

            - optsdepends: displays the service's optional dependencies

            - extdepends: displays the service's external dependencies

    - 66-shutdown skeleton: be safier and check if options are past

    - New tool:

        - 66-update: The 66-update program makes a complete transition of trees and the live directory using a old 66 format (the one being replaced) with the new 66 format.

---

# In 0.2.3.2

- Fix bad annoucement at 66-svctl

- Warn in case of bad key at parenthesis parsing process

---

# In 0.2.3.1

- Bugs fix: bad allocation memory

- Makefile: remove creation of empty directories. 66-tree will check all directories and create it if it missing.

---

# In 0.2.3.0

- Adapt to oblibs 0.0.4.0

- Remove RB_DISABLE_CAD at 66-boot by default and allow to enable it with -c option. Starting on virtual system like lxc crash with this option. Well be safe on every case.

- Use of new function log_? familly from oblibs:

    - standardization of the exit code

    - standardization of the verbosity output

    - allow verbosity to 4 to display debug message

- Doc typo fix

---

# In 0.2.2.2

- Adapt to skalibs 2.9.1.0

- Remove insta_? deprecated function

---

# In 0.2.2.1

- Typo fix at html documentation

- Fix 66-intree without options

---

# In 0.2.2.0

- Bugs fix: Always check the existing of the 66 heart directories

- 66-info is now deprecated and splitted into two differents API:

    - 66-intree give informations about tree

    - 66-inservice give informations about services

---

# In 0.2.1.2

- Hot fix: fix 66-info graph display.

---

# In 0.2.1.1

- Hot fix: fix the build of the service dependencies graph.

---

# In 0.2.1.0

- Bugs fix.

- A synchronization is now made on reboot even with force option.

- The field @name has no effects except for instantiated service but can be omitted. The name of the frontend file is took as name. In case of instantiated service, the field @name must contain the complete name of the frontend service file -- Refer to the frontend documentation page.

- New skeleton file: rc.shutdown.final. This skeleton file will be run at the very end of the shutdown procedure, after all processes have been killed and all filesystems have been unmounted, just before the system is rebooted or the power turned off. This script normally remains empty.

- Extra tools has removed and provided as an independent programs at ttps://framagit.org/Obarun/66-tools.git except for 66-echo and 66-unmountall program which are a dependent part of 66.

- The account to run the s6-log program at the associate service logger can be set at compilation time by the --with-s6-log-user=USER options. The default is root. Obviously, the @runas field at the [logger] section overwritte it. This options set the account to run the uncaught-logs too. Also, this option can be overwritten with the '-l' option at 66-boot invocation and the '-L' option at 66-scandir invocation.

---

In 0.2.0.4

- Bugs fix on memory allocation

- Respect policies decision of user:

    - Remove -m option from 66-boot on init file

- Add -d feature to 66-scandir

---

# In 0.2.0.3

- 66-parser: Fix write of configuration file, add -c|C features

---

# In 0.2.0.2

- Fix the written of user configuration file

- Fix oneshot and bundle status check

- Add -l features to 66-tree, rewrite tree_unsupervise function

---

# In 0.2.0.0

- New tools:

    - 66-boot.

    - 66-scanctl.

    - 66-shutdown.

    - 66-shutdownd.

    - 66-hpr.

    - 66-env.

    - 66-parser.

    - 66-which.

    - 66-echo.

    - 66-unmountall.

    - execl-subuidgid.

- New @key field and change on frontend file:

    - [Logger] section accept a @depends field, @timestamp accept none as value, readiness notification is a default.

    - @hiercopy in [main] allow to copy any file or directory coming from the directory of the service.

- 66-envfile is now deprecated, use execl-envfile in replacement.

- 66-enable: add -F, -c, -C features.

- 66-svctl is now asynchrone to launch services.

- Man pages are available.

- Environment variables limitation: maximum 100 files by directory, each file cannot contain more than 4096 bytes or 50 variables.

- Syntax to unexport variable with execl-envfile change: the exclamation mark '!' need to be placed at begin of value instead of key.

- Bugs fix

---

# In 0.1.0.1

- Bugs fix

- Add 66-getenv,66-writenv and 66-gnwenv extra-tools

---

# In 0.1.0.0

- Logger for nested tree at scandir creation time is disabled.

- Pass graph function to new ss_resolve_graph_? function.

- Split resolve file and state flags.

- Add ss_state_? function.

- Really unsupervise rc service, add rc_manage, rc_unsupervise function.

- Fix -U option for 66-tree.

- Fix 66-info, add -c option.

---

# In 0.0.2.2

- Hot fix, do not stop an empty db.

---

In 0.0.2.1

- Hot fix, do not append inner bundle with empty word.

---

# In 0.0.2.0

- Bugs fix.

- Add 66-tree -U options to unsupervise a tree from a scandir.

- Rewrite 66-info to provide color and more informations.

- Empty database cannot be initialized.

- Respect /etc/66/sysadmin/service even for user.

---

# In 0.0.1.1

- Bugs fix at rc_init function.
