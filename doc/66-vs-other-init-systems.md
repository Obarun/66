# Where 66 stands among init systems

An honest placement of 66 next to runit, the s6 stack, OpenRC, finit, dinit and systemd:
what it is alone in doing, what it deliberately does not do, and what the remaining gaps
would actually cost to close.

## The grid that matters

Most init comparisons are shouting matches about size. Two axes are enough to place
everyone without arguing about taste.

**Axis A, the supervision model.** One supervisor process per service (PID 1 stays tiny,
supervision survives a crash of the manager) versus one monolithic manager that watches
every service itself.

**Axis B, the richness of the service model.** Raw shell scripts, versus a complete
declarative model, versus a full system layer that also owns login, network, DNS,
logging and an IPC bus.

Crossing them places everyone. Axis B runs down the rows, widest scope at the top and
poorest model at the bottom; axis A splits the columns.

| Axis B ↓ / Axis A → | **one supervisor per service** | **one monolithic manager** |
|---|---|---|
| **System platform**: also owns login, network, DNS, logging, an IPC bus | *empty* | systemd |
| **Complete declarative model**: dependencies, instances, groups as objects, readiness, user scope | **66** | dinit · finit |
| **Declarative, narrower**: a dependency graph over a script-shaped model | s6 stack | — |
| **Scripts**: no native model | runit | OpenRC *(no native supervision at all)* |

Two things fall out of it.

66 holds a cell nobody else occupies. Everything else in its column, runit and the s6
stack, has the supervision model but a much poorer service model. Everything else on its
row, dinit and finit, has a comparable service model but a manager that supervises every
service itself, so a bug in the manager takes the whole supervision tree down with it.
Nobody else has both.

The row above is a different job, not a higher grade. systemd is one row up because it owns
login, the network, name resolution and an IPC bus: scope, not refinement. Reading that row
as "more advanced" is what turns the comparison into a shouting match. And the top-left
cell is empty. Nobody has built a system platform on a one-supervisor-per-service
architecture, and 66 is not trying to; see *Placement*.

## What it actually looks like

Before the argument, the object. Everything below is the real syntax; a reader who has
never seen 66 can judge the rest against it.

A service is one file. Here is a supervised daemon that also restarts itself whenever the
DHCP client rewrites `/etc/resolv.conf`:

```ini
[Main]
Type = classic
Description = "dnsmasq daemon"
Depends = ( network )

[Start]
Execute = ( /usr/bin/dnsmasq -k )

[Event]
EventType = inotify
From = ( resolv-watch )
Do = restart
```

Everything about dnsmasq is in the file called dnsmasq, including how it reacts. There is
no companion unit beside it.

The `[Event]` block names a source, and a source is itself a service, of a type that runs
no process and only emits:

```ini
[Main]
Type = event
Description = "watch /etc/resolv.conf"
EventType = inotify
Watch = /etc/resolv.conf
On = ( IN_CLOSE_WRITE )
```

`Execute` is an execline script by default, but a shebang on its first line switches
language: the same daemon in bash or Python is the same file with
`Execute = (#!/usr/bin/bash …)`. Bring it up with `66 start dnsmasq`; there is no separate
parse step, `start` does it. A logger is attached by default, so `66 status dnsmasq` reports
the pid, the uptime, the tree it landed in, and the last lines it printed.

That is the shape of the whole system: one INI file per service, groups of services as
trees, and a service's reaction written inside the service it concerns.

## Who this is for

The answer differs depending on which chair you are sitting in, and conflating the two is
how these comparisons usually go wrong.

**As a user of a distribution that ships 66,** there is nothing to take on. Services are
installed by packages, they start at boot, and the init system is as invisible as any
other. Nothing above is homework: a frontend with no tree named lands in the default tree
on its own. You write a file only when you want a service nobody packaged, and then you
write the ten lines shown above, once.

**Good fit if** you administer machines you actually reason about: a workstation you tune,
a fleet you want reproducible, a container image you want deterministic. Especially if you
have ever wanted to know *why* a machine came up the way it did and found that the only
answer available was to run an analyser after the fact. The two things 66 gives you that
nothing else here does are a graph frozen before boot and an ecosystem you can move between
machines as one artefact.

**Good fit if** you build a distribution or an appliance and want the boot sequence to be a
reviewable object rather than an emergent one.

On packaging, the honest position is narrower than it is usually stated. Upstream
projects do not ship 66 frontends, but they do not ship runit, OpenRC, dinit or s6-rc
descriptions either; upstream ships systemd units and nothing else, which is a property of
systemd's reach rather than of anyone's model. What matters is whether a catalogue exists,
and one does. [`66-service`](https://git.obarun.org/66-service) carries about seventy-five
ready frontends (sshd, nginx, postgresql, dockerd, NetworkManager, wpa_supplicant,
pipewire, bluetooth, cups, four display managers, libvirtd, samba and so on) written to be
distribution-agnostic rather than Obarun-specific, alongside the `boot@` module that has
been proven on several distributions.

Adoption is not Obarun-only either: antiX 26, built on Debian trixie, ships 66 as one of
five selectable init systems. The residual cost is real but bounded. What the catalogue
does not cover, you write, and that is the file shown above.

**Bad fit if** you depend on the systemd *platform* rather than on systemd the service
manager: `logind` integration over D-Bus, socket activation, `systemd-networkd`, `resolved`,
or software that links `libsystemd` and expects the rest to be there. Parts of that have
answers in the 66 suite and parts do not; pretending otherwise wastes your week.

## Feature comparison

Native capability, not what can be bolted on with glue. The *s6 stack* column covers
s6 + s6-rc + s6-linux-init, driven by the `s6` command of s6-frontend 0.1.0.0 — the
frontend is a CLI over that stack, so the capabilities are the stack's.

| | runit | s6 stack | OpenRC | finit | dinit | systemd | **66** |
|---|---|---|---|---|---|---|---|
| Runs as PID 1 | yes | yes (`s6-svscan`, via s6-linux-init) | no (service manager) | yes | yes | yes | yes |
| One supervisor per service | ✔ | ✔ | ✖ (delegates) | ✖ | ✖ | ✖ (cgroup-tracked) | ✔ |
| Dependencies | none | DAG (services only) | `depend()` in shell | conditions | typed DAG | DAG (units only) | **two DAGs** — services (+ `Conflict` / `Provide` / `OptsDepends`) *and* trees |
| Service description | scripts | source directories, compiled into **one database for all services** | shell | INI-like | key=value | INI units | INI frontend compiled to **one CDB per service** (+ addons) |
| Graph computed | n/a (no dependencies) | **at compile time** | at boot (shell) | at boot | at load time | at boot, after generators run | **at parse time, frozen in CDB** |
| Readiness protocol | ✖ (`./check` polls, no notification) | ✔ (fd) | partial | ✔ | ✔ (fd) | ✔ (`sd_notify`) | ✔ (`Notify`) |
| Templated instances | ✖ | ✔ — s6 *dynamic* instances: created at run time in a nested supervision tree, outside the compiled database | ✖ | ✖ | ✖ | ✔ | ✔ (`@`) — materialised at parse time as ordinary services, inside the graph |
| Reified service groups | ✖ | bundles (aliases) + offline sets | runlevels | runlevels | ✖ | targets (ordering only) | **trees, with dependencies of their own + encapsulated modules** |
| First-class user scope | ✖ | ✔ (`s6 -u`) | ✖ | ✖ | ✔ | ✔ | ✔ |
| Logging | per-service svlogd | per-service s6-log | syslog | syslog | file / buffer | central binary journal | per-service chained logger + `66 log` |
| Process containment | ✖ | ✖ (planned package) | ✖ | cgroups | ✖ | cgroups | **PID namespace (`66-ns`)** |
| Resource control | ✖ | ✖ (planned package) | cgroups | cgroups (v2) | ✖ | cgroups | **✖** (rlimits only — on the roadmap) |
| Socket activation | ✖ | ✖ | ✖ | inetd | ✖ | ✔ | **✖** |
| D-Bus activation | ✖ | ✖ | ✖ | ✖ | ✖ | ✔ | **`66-dbus-launch`** — bus service files translated to frontends, activation request → `66 start` |
| Sandboxing | ✖ | ✖ (planned package) | ✖ | limited | limited | namespaces + seccomp | caps, rlimits, no-new-privs + namespaces (`66-ns`) |
| Seats & sessions | external | external | external | external | external | logind (D-Bus) | `66-userd` + `seatd`, no D-Bus |
| Events / timers | ✖ | ✖ (ftrig is a primitive, not a rule engine) | ✖ | conditions | ✖ | `timer` / `path` units | **`[Event]`: `service`, `signal`, `inotify`, `schedule`, `user`, `timer`** |
| Definition layering | ✖ | stores — list read from `s6.conf`, + masking | ✖ | ✖ | ✖ | unit directory search path + `mask` (unloadable) | **admin > packaged, first match wins** (three levels in user scope); paths fixed at build, explicit per-command override |
| Snapshot & restore | ✖ | offline sets — a selection, not the definitions | ✖ | ✖ | ✖ | ✖ | **the whole ecosystem, transferable and exactly restorable** |
| Typical adoption | Void, Artix | Artix, Adélie | Gentoo, Alpine | embedded | Chimera, Artix | nearly everything | Obarun; antiX 26 (one of five selectable inits) |

## What 66 is alone in doing

**Trees, and a second dependency graph over them.** The service space is partitioned into
named groups, enabled, disabled, started and stopped as a block, several of them live at
once. The part that is easy to miss is that a tree is not merely *ordered* against its
neighbours: it carries `depends` and `requiredby` of its own, resolved through its own
DAG, by the same machinery that resolves services, with the same resolved fields on disk
and the same two-directional traversal. 66 runs two dependency graphs at two levels:
services inside a tree, and trees among themselves.

The two graphs are not peers. The service graph is global and authoritative: it crosses
tree boundaries and always wins. If a service in one tree depends on a service held by
another tree that is scheduled to come up later, the dependency is still started first and
the tree ordering yields. Trees arrange blocks; they never reorder a dependency. That
answers the obvious objection to reified groups, which is that putting services into
ordered containers should let an administrator write a grouping that contradicts the
dependency graph and then either deadlock or start something too late. The contradiction
simply resolves: the coarse graph never overrides the fine one, so a tree layout can be
inconvenient but not incorrect.

That is what makes the group a container rather than a label, and it is where the usual
comparisons miss the point. systemd targets do have dependencies, but a target is not a
container, so those dependencies are simply more unit-level edges in the one flat graph.
There is no graph *over* the groups because there are no groups, only units that happen to
pull others in. Runlevels, at the other extreme, are ordered by integer and cannot express
anything else.

And s6-frontend's **sets**, which look closest of anything in this comparison, are a
different object: a set holds *every* service the stores know about, each carrying a boot
disposition (masked, usable, active, essential); it is edited offline, checked for
consistency, compiled, then installed, and exactly one is live at a time. A set answers
"what should this machine run at boot". A tree answers "which live block does this service
belong to". Both deserve their name; neither substitutes for the other.

**Boot and runtime are separate objects, not separate conventions.** Bringing the machine
up and running services on it are different jobs, and in 66 they live in different trees.
The shipped seeds say so outright: `contributions/seed/boot` declares `groups = boot`,
`contributions/seed/session` declares `groups = admin` and `depends = global`. The
partition is the default the project ships, not a discipline each administrator has to
invent. Every operation that could cascade (stop, free, disable a block) names a tree, and
a tree does not enumerate what is not in it.

Neither of the two comparable systems partitions anything, and both need a fence instead.

s6-rc compiles one database holding every service, boot infrastructure included, in one
flat graph, and `s6-rc -d` propagates upward through reverse dependencies. The fence is
`flag-essential`, a file dropped in a service definition directory: with it present,
"s6-rc will refuse to stop that service". Only `-D` overrides, and their own documentation
says that option "should only be used when the machine is going to be stopped". That is a
per-service veto compensating for the absence of a boundary. The graph cannot express
"this belongs to the machine's foundations", so each foundation service has to carry a flag
saying "not me".

systemd is flat too. Its nearest equivalent is `DefaultDependencies=`, true by default
(`unit.c:108`), which silently injects `After=` / `Requires=sysinit.target` and `Before=` /
`Conflicts=shutdown.target` into every service (`service.c:764,785`); early-boot units opt
out with `DefaultDependencies=no`. So the boot/runtime distinction exists, but as a
per-unit opt-out from edges the manager adds behind the author's back. That is the same
objection as the generators in a different guise: the graph is not what the unit files say.

The cost on the 66 side is that a partition has to be *assigned*. Somebody, the
distribution's seeds or the administrator, decides which tree a service belongs to, and a
wrong assignment is a real mistake to make. A flat graph never asks the question. It also
never lets you answer it.

**Modules.** A group of services handled as *one* encapsulated unit, with instance
substitution inside it. Everywhere else a group is a label on a set of independent units;
in 66 it is an object with a boundary.

**Live version migration.** The manager can be upgraded and *its own* persistent state
converted in place, under a running system: 0.8.x resolve files become 0.9.0.0 ones
without a reboot. No other project here attempts that.

**Snapshots**, which are not merely named configurations. A snapshot carries the ecosystem
itself: the frontend definitions at all three levels (system, administrator, user), their
configuration and environment files, the scripts, the seeds that describe trees, and the
resolved system state. One artefact, the whole thing. That buys two properties a
*prescription* structurally cannot have:

- **Transfer.** Dropped on another machine, a snapshot reproduces the same ecosystem
  there, with the guarantee that services run against strictly the same definitions rather
  than against whatever the local packages happen to provide. Reproducibility across a
  fleet is a property of the artefact, not of the discipline of whoever populated the
  machines.
- **Exact restore.** Coming back means coming back to a working state, byte for byte,
  definitions and configuration and environment included, not to a selection that will
  resolve against whatever is installed today.

Which is where a set, described above, differs in kind rather than in degree: it is a
selection over definitions that live outside it. Copy one to another machine and it means
something only if that machine's stores already hold the same definitions; the set does not
carry them, and cannot guarantee them. A snapshot *is* the definitions. One is a choice
about an ecosystem, the other is the ecosystem. `s6-rc-update`, similarly, swaps a live
service database while services keep running, which is a different operation again.

**Event reactors.** One `[Event]` block, written inside the service that cares about it.
`EventType` picks the source (`service`, `signal`, `inotify`, `schedule`, `user`, `timer`),
`On` and `OnAll` give the condition, and `Do` gives the action: `start`, `stop`, `restart`,
`reload`, `reconfigure` or `free`. A separate `Emit` key lets a rule publish an event of its
own, so reactors chain. It is fed by internal milestones (`boot.done`, `shutdown.begin`) as
much as by `66 emit` from userland.

systemd covers part of that ground with two extra unit types, `path` and `timer`, and the
gap is wider than the missing sources. A service there cannot react on itself. A `path` or
`timer` is a *separate* unit, and the only action it can take is to activate the unit it is
paired with. It cannot restart, reload or reconfigure it, and there is no way to write,
inside a service, a rule about that service. In 66 the rule lives in the service it
concerns and acts on it: `Do = reload` on a `SIGHUP` is the service responding to itself,
in its own frontend file.

**A PID 1 that does almost nothing**, which is not 66's alone. It is shared with runit and
above all with the s6 stack, from which the model is inherited and which the table credits
with it too. Kept through the native rewrite: the scanner spawns one supervisor per
service, and the supervisors are what keep services alive. The CLI, the graph resolver and
the parser are ordinary programs that can crash without taking supervision down.

## Compiled ahead of time, not parsed at boot

This is a property of the pipeline rather than a feature, and it decides what the other
guarantees are worth.

In 66 the parser runs when the administrator says so, at `66 parse` or at `66 enable`, and
its output is a CDB: a static, compiled description of the service. Boot does not parse, it
reads compiled artefacts.

That guarantee comes from an ordering property, and it should be stated exactly rather than
too broadly. `66 start` *can* parse: hand it a service that has never been parsed and it
will do the work on the spot, which is the right behaviour on the interactive path. It
never happens at boot, because a service only comes up at boot if it was enabled, and
enabling is what runs the parse. So the boot path cannot reach a service whose CDB does not
already exist. The parser is not absent from the binary; it is unreachable from the path
where reproducibility matters.

Three things follow, and they are consequences rather than promises:

- **A given start is always the same start.** Not "should be", not "as long as nothing
  drifted". There is no code path in which a frontend file is re-interpreted while the
  machine is coming up, so there is nothing to drift.
- **The dependency graph is frozen at parse time.** It is a thing on disk that can be
  inspected, diffed and shipped, not a computation whose result depends on when you ran it.
- **It is what makes a snapshot mean anything.** A snapshot carries the compiled state, so
  "the same ecosystem on another machine" means the same *resolved* graph, not the same
  inputs re-resolved locally and hoped to agree.

systemd is built the other way round, by construction rather than by oversight. Units are
loaded from a search path at runtime, and *before* they are loaded, generators run, at
every boot and at every `daemon-reload`. They are ordinary executables that synthesise,
alter and mask units. Eleven ship in the tree, and what they read is machine state:
`debug-generator` takes `systemd.mask=` and `systemd.wants=` straight off the kernel
command line (`debug-generator.c:42,55`), `gpt-auto-generator` probes the partition table
with blkid, `fstab-generator` reads `/etc/fstab`. Third-party generators are supported and
common.

So the unit set, and therefore the graph, is a function of the machine's state at the
moment of boot, recomputed every time. Two boots of a machine nobody touched can differ
because the kernel command line, a partition table or a dropped-in `.conf` differed. There
is no compiled artefact to review; `systemd-analyze dump` reports what the graph *became*,
after the fact. That is the difference between a guarantee and an observation.

s6-rc compiles too: `s6-rc-compile` produces a service database, and `s6 set commit`
compiles a set into one. Compile-ahead is a design school with two members here rather than
a 66 exclusive, which makes it a stronger point and not a weaker one. But the granularity is
opposite, and it decides everything downstream. s6-rc compiles one database for all
services. 66 compiles one CDB per service, finer still, with separate addons per concern
beside it: `.execute`, `.dependencies`, `.environ`, `.io`, `.limit`, `.regex`.

The consequences all run the same way:

- **Unit of change.** Touching one service in 66 rewrites that service's artefact. In
  s6-rc it means recompiling the whole database and swapping the live one.
- **Blast radius.** One malformed definition fails one parse and leaves everything else
  intact. A monolithic compile either succeeds whole or yields nothing.
- **Ceremony.** This is *why* the s6 stack needs sets, `commit`, `install` and `apply` at
  all; eight `s6-rc-set-*` binaries exist to manage those selections. When the artefact is
  the entire machine, changing one line requires an offline staging area and an install
  step. When the artefact is the service, `66 enable foo` is just enabling foo.
- **Composability.** A per-service collection of CDBs can be added to and taken from, which
  is what lets a snapshot be an ecosystem rather than a blob.

The other side has something for it. Compiling the whole database buys whole-system
consistency by construction: the live database cannot contain a dangling dependency,
because it was validated as a unit. 66 has to obtain that from graph resolution instead.
That is where the cost of per-service granularity is paid.

## The division of labour

Several things systemd absorbed into PID 1 exist in the 66 world as separate programs of
the suite, and reading the core alone makes them look missing:

- **Confinement** is `66-ns`: mount, PID, user, network, UTS, IPC and cgroup namespaces,
  declared as typed elements or rule files and composed through the service's `Execute`
  line.
- **Sessions** are `66-userd`: a PAM module reports login and logout, the daemon tracks
  who is logged in and from where, owns each user's runtime directory, applies the
  shutdown policy, and brings that user's 66 services up on the first session and down on
  the last, **without D-Bus**.
- **Seats** are `seatd`: the hardware context, devices and VTs.
- **D-Bus activation** is `66-dbus-launch`: it launches and supervises `dbus-broker` as its
  controller, translates every D-Bus service file it finds into a 66 frontend placed in the
  `dbus` tree, and turns an activation request from the bus into a plain `66 start`. A
  `SIGHUP`, or `66 reload dbus`, resynchronises: new bus files are translated and
  activated, removed ones are `66 remove`d and deactivated, changed ones re-parsed. It
  handles `UpdateActivationEnvironment()` and `ReloadConfig()`, and runs per-user as
  readily as for root, dropping privileges before exec'ing the broker.

So the answer to "where is logind" is: split between `66-userd` and `seatd`, along the
boundary of what each piece actually owns, and the same reflex accounts for the other two
entries above. That is the supervision model applied to the suite itself: no single process
has to be trusted with all of it.

The activation case repays a second look, because it shows the reflex paying off. There is
no activation *unit type* in 66 and no special case in the core: a bus-activated service
becomes an ordinary frontend, parsed to CDB like any other, started by the ordinary `start`
path, supervised by the ordinary supervisor. The translation step is admittedly a cousin of
a systemd generator, since it derives service definitions from foreign files, but it runs at
a moment the administrator chooses, and what it emits is readable frontend files on disk
rather than units materialised in a manager's memory. Two limitations come from its own
documentation: dbus-broker's policy API is not yet stable, so access is broad, and D-Bus's
own `system.conf` / `session.conf` are not interpreted.

## Readiness and events: who is allowed to speak

The feature table gives 66 and systemd the same tick for "readiness protocol". The tick
hides the most instructive disagreement in the whole comparison. Both projects answer the
same question, *when is a daemon actually ready?*, and the difference is not in the answer
but in which direction the information travels. 66 is not alone on its side: the descriptor
protocol is s6's design, which 66 kept through the rewrite, and dinit arrived at the same
shape independently. Both are credited `✔ (fd)` in the same table. systemd is the outlier
here, not 66.

### The readiness contract

Both agree that `exec` succeeding proves nothing and that `Type=forking` is a poor proxy.
The daemon has to say so itself.

- **66** passes a pre-opened descriptor to the service (`Notify`). The daemon writes one
  `\n` on it. Its supervisor reads it and the service is up.
- **systemd** creates an `AF_UNIX` datagram socket, exports its path as `NOTIFY_SOCKET`,
  and the daemon sends `READY=1` to it. The same socket also carries `STATUS=`,
  `MAINPID=`, `RELOADING=1`, `STOPPING=1`, `WATCHDOG=1`, `EXTEND_TIMEOUT_USEC=`,
  `FDSTORE=1` with `SCM_RIGHTS`, `ERRNO=` and `BARRIER=1`.

The socket buys one real thing an inherited descriptor cannot: ambient reachability. An
environment variable crosses `fork` and `exec` for free and survives the classic
`closefrom(3)` in a daemonizing routine, which silently destroys a notification
descriptor. Any descendant, at any depth, can still notify. That is a genuine advantage
and it should be conceded plainly.

Everything else systemd carries on that channel is the invoice for it:

| Because | It needs |
|---|---|
| anyone can write to the socket | peer authentication: `SO_PEERCRED`, `NotifyAccess=` |
| the sender may not be the daemon | `MAINPID=`, which lets a service redefine which process the manager tracks |
| the sender may exit before the manager reads | `BARRIER=1` plus a descriptor passed by `SCM_RIGHTS` |

And none of it closes the hole. The man page for `systemd-notify(1)` ends up recommending
that you call `sd_notify()` in-process instead, which means giving up ambient reachability,
the only thing the socket was selling. The design refutes itself in its own documentation.

The clearest evidence is external to 66. s6 ships two bridges between the two protocols,
and their asymmetry is the whole argument. `s6-notify-fd-from-socket` converts systemd to
fd: it listens for one thing, `READY=1`, discards every other key, and writes a single
byte. `s6-notify-socket-from-fd` converts fd to systemd: it must *synthesise* a `MAINPID=`
the source protocol never needed to name, then add a `BARRIER=1` round trip to compensate
for an authentication problem the source protocol never had. One direction is a filter that
throws away. The other is a filter that manufactures. The poorer protocol is the semantic
superset.

One more property is rarely mentioned: a descriptor gives EOF for free. If the daemon dies
during initialisation, the read end sees EOF immediately, so "not ready yet" and "dead" are
distinguishable with no timeout and no knowledge of any pid. On a stateless datagram socket,
silence means nothing, so the manager needs a second, out-of-band source of truth: `SIGCHLD`
on the main pid. Which requires knowing the main pid. Which is why `MAINPID=` exists.
systemd only escapes the circle because it is PID 1 and reaps every orphan.

### The event bus is a separate channel

Where systemd runs every one of those semantics down a single pipe, 66 keeps two
mechanisms apart. Readiness is the descriptor, and nothing else. Events are `66-eventd`,
which never participates in readiness: a service does not become ready by emitting. The
bus reports what *happened*, not what a process *claims to be*.

That separation is enforced by who is allowed to produce. Lifecycle events are emitted by
the supervisor (`66-supervise.c` lines 339, 1021, 1290 and 1344) from what it knows
first-hand: its own `waitpid`, the notification descriptor it holds, the signal it sent.
The supervised service has no way to speak about itself on the bus.

This inverts the systemd flow exactly. There, the process *declares* its state and the
manager decides whether to believe it. Here the state is *observed* by the one party that
holds it. That is why there is no equivalent of `MAINPID=`: the supervisor knows the pid,
nobody needs to tell it, and crucially nobody is permitted to redefine it. Across the
entire lifecycle path there is simply nothing to authenticate, because the question never
arises.

### Transport chosen per problem

66 does use a socket, `eventd/s`, for the one job a descriptor structurally cannot do:
accepting events from arbitrary clients not known at start-up (`66 emit`). N unknown
producers require a named rendezvous. But the transport is a *connected stream*, and the
check happens at accept time (`66-eventd.c` lines 1295–1314): `socketunix_getucred` on a
live peer, rejection unless `cred.uid` matches the daemon's owner. No race against a
sender that has already exited, no recycled pid, and no barrier to invent. The stream is
the barrier.

| Need | Producers | Transport | Authentication |
|---|---|---|---|
| readiness | one, known, direct child | inherited fd | none needed, structural |
| lifecycle → bus | one, the supervisor | fifo directory | none needed, the producer is the trusted party |
| external injection | N, unknown | connected stream | at accept, on a live peer |

systemd has one row for all three needs. Everything else follows from that.

The sharpest consequence is where each channel sits relative to a privilege boundary.
66's socket is same-uid to same-uid: a process able to reach it can already run
`66 start`, so `66 emit` grants no authority the CLI did not already grant, and a coarse
uid check is exactly the right granularity. systemd's channel runs *service to PID 1*, and
the writing process is often one the unit deliberately weakened with dropped privileges, a
sandbox, `DynamicUser=` or a namespace, yet it retains a path to the most privileged
process on the machine, with `MAINPID=` and `FDSTORE=1` on it. The channel crosses the very
boundary the unit just erected.

Two caveats. First, `66-eventd` does *more* than `sd_notify`, not less: a reactor turns an
event into an action, which is a far larger surface than a readiness protocol ever had. The
claim here is not "simpler" but "the transport was chosen per problem instead of once for
all of them". Second, neither model helps a daemon that has no notion of readiness at all
and accepts no descriptor; that case needs external probing on both sides, `s6-notifyoncheck`
in one world and a blocking `ExecStartPost` in the other. The merit of the descriptor is in
the contract, not in the coverage.

## What 66 does not do

1. **No cgroup resource control** *yet*. There is no memory ceiling, CPU weight, task cap
   or per-service accounting; rlimits are per-process, and `RLIMIT_NPROC` counts per user,
   not per service. This is a control gap, and it is **on the roadmap**. Why it is *not*
   also a tracking gap, and what closing it costs, is worked out in
   [*What cgroups and seccomp would cost 66*](https://git.obarun.org/Obarun/66/-/blob/master/what-cgroups-and-seccomp-would-cost.md).
2. **No socket activation.** The fd holder keeps descriptors across restarts; that is not
   lazy activation. D-Bus activation, by contrast, *is* covered; see the division of
   labour below.
3. **No seccomp**, in the core or in `66-ns`. Hardening stops at capabilities, rlimits,
   `no_new_privs` and namespaces.
4. **Linux only.** Not by architecture, but for one missing event-loop backend; see
   *Portability* below.
5. **Ecosystem reach.** No upstream project ships a 66 frontend, as none ships runit,
   OpenRC, dinit or s6-rc descriptions either, so the catalogue is maintained downstream.
   [`66-service`](https://git.obarun.org/66-service) covers roughly seventy-five of the
   common ones; beyond that, the distribution writes them.
6. **Maintenance surface.** lib66 is around 256 `.c` files, and the native rewrite of the
   supervision chain (scanner, supervisor, logger, svctl) adds surface that systemd
   amortises over hundreds of contributors. That is a project risk, not a technical one,
   and it should be stated plainly.

## Portability

66 runs on Linux only, and the reason is not the one people assume. 66 is not
architecturally tied to Linux; it is tied to one event loop that has only ever had a Linux
backend.

**66's own non-portable surface is tiny.** The complete inventory of OS-specific headers
under `src/`:

| Header | Uses | Where | Half |
|---|---|---|---|
| `linux/capability.h` | 4 | `lib66/linux/caps.c`, `enum_parser.c`, headers | manager |
| `sys/prctl.h` | 2 | `66-execute.c` (no-new-privs), `caps.c` | manager |
| `sys/mount.h` | 2 | `ssexec_boot.c`, `shutdown/umountall.c` | **init** |
| `sys/reboot.h` | 2 | `66-hpr`, shutdown | **init** |
| `linux/kd.h` | 1 | `ssexec_boot.c` (console / VT) | **init** |
| `sys/inotify.h` | 1 | 66-eventd's inotify source | manager |
| `sys/signalfd.h` | 1 | `66-log.c` | manager |

Plus three `/proc` paths: `cmdline` and `mounts`, both in the init half (`kenv` and
`getmntinfo` on FreeBSD), and `/proc/self/fd`, which exists as `/dev/fd` everywhere.
Notably, `epoll_create`, `timerfd_create` and `eventfd` appear nowhere in 66. Not one
direct call.

**The actual blocker is one layer down.** The whole runtime (66-scandir, 66-supervise,
66-log, 66-eventd, 66-fdholderd, `svc_launch`, `tree_launch`) runs on oblibs' SSE loop,
which is built on the five Linux mechanisms: epoll, signalfd, timerfd, eventfd and
inotify. That is what pins 66 to Linux, and it lives in oblibs, not here.

The shape of that debt is favourable. oblibs' `event/` directory is 12 files and about
2300 lines, and all seven `epoll_*` calls in the project sit in a single file, `sse.c`.
The demultiplexer is already behind an abstraction; the watchers above it are fd sources.
And this is the direction in which kqueue is *simpler* than epoll, not harder:
`EVFILT_SIGNAL`, `EVFILT_TIMER`, `EVFILT_USER` and `EVFILT_VNODE` replace the four
Linux fd types natively, with no descriptor at all. Four fd-based watchers become four
filters. epoll is the late, partial imitation of kqueue; porting this way runs with the
grain.

It is also the price of independence. skalibs was the layer that carried these
abstractions onto the BSDs; 0.9.0.0 removed it in exchange for control, and portability
left with it.

And "portable" means less than it sounds, for everyone. dinit on macOS is not PID 1;
it is a session service manager there and nothing more. On FreeBSD, being PID 1 obliges
it to reimplement the init role locally, exactly as 66 would. The split is the same on
both sides:

- **The manager half** (parser, DAG, trees, resolve/CDB, modules, instances, logger,
  snapshots) is plain POSIX. Nothing to port. That is most of lib66.
- **The init half** (`ssexec_boot`, `umountall`, `66-hpr`, the console) is
  platform-specific for *everyone*, dinit included. That is not portability, it is a
  per-platform rewrite by nature.
- **Capabilities** are a Linux concept. FreeBSD's Capsicum is a different model, not an
  equivalent; macOS has nothing. `CapsBound` / `CapsAmbient` would be compile-time gated.

| Target | Work | Cost |
|---|---|---|
| kqueue backend in oblibs | one file beside `sse.c` + four watchers on native filters | 1–2 weeks |
| 66 as a service manager on FreeBSD/macOS | gate capabilities, gate the init half | a few days |
| 66 as a FreeBSD init | `ssexec_boot`, `umountall`, `hpr` natively (`getmntinfo`, `kenv`, `reboot(2)`) | a project of its own |

So: **66 as a service manager on FreeBSD or macOS is roughly two to three weeks, and most
of it happens in oblibs rather than in 66.** 66 as a FreeBSD init is a separate
undertaking, the same one dinit had to carry out.

## Placement

66 is **the only service manager that offers a systemd-grade declarative model on an
s6-grade supervision architecture.** Two projects sit closest, for opposite reasons, and
neither is systemd.

**s6-frontend is the nearest architecturally.** The s6 stack now has its own unified `s6`
command, with the same offline/live split, the same enable/disable vocabulary, a per-user
mode and the same 0/100/111 exit convention, over the same one-supervisor-per-service
model.

The chronology runs the opposite way from what that sentence suggests. 66 has existed
since 2018, and its single unified `66` command dates from 0.8.0.0, tagged October 2024;
s6-frontend's first release is January 2026 and its 0.1.0.0, the one that adds user mode
and `apply`, is July 2026. So 66 reached that shape about fifteen months earlier. Instances
are the older case and the wider gap: 66 shipped `@I` in 0.2.1.0, September 2019; s6
implemented instances in 2.11.2.0, January 2023, more than three years later.

Two datapoints are not a law, and neither project is copying the other. Both are solving
the same problems from the same supervision model, which is exactly why they converge. But
the direction of travel should be stated plainly, because the reading order of this
document invites the opposite assumption: where the two have arrived at comparable
features, 66 arrived first.

The distance that matters is therefore not in the CLI, and s6-frontend's own overview says
so plainly: it "does not come with any innovating concepts — it's just a series of
user-friendly wrappers around various commands in the s6 ecosystem". That is an accurate
description of a frontend, and it is also the measure. The two projects converged on how a
service manager should be *driven*, and diverge entirely on what it should *model*.

What separates them is the *service* model, not the supervision model: s6-rc source
directories compiled into one database for the whole machine against an INI frontend
compiled per service, bundles that alias a group against modules that encapsulate one, no
live partition of the service space, no event engine, no snapshot that carries the
definitions. Plus the dependency stance, which now points in opposite directions. 66
0.9.0.0 depends on exactly one library, oblibs, which is Obarun's own and is itself free of
skalibs and execline; s6-frontend requires five external packages: skalibs, execline, s6,
s6-rc and s6-linux-init. The difference is not "none versus five", it is a single library
the project controls against a stack it consumes.

Two things it has that 66 does not, and they are real: whole-database consistency by
construction, discussed above, and portability, inherited from skalibs.

Stores are the third candidate, and they do not survive inspection. Layering itself is
not the difference. 66 already searches administrator directories before packaged ones, and
user directories before both, first match winning; a packaged definition and a local
override coexist there exactly as they do in the s6 stack. What differs is two details, and
neither points the way it first appears.

The store *list* is configuration in s6-frontend: `/etc/s6.conf` carries a `storelist` key
read at run time. In 66 the paths are fixed at build, and an absolute path handed to
`66 parse` overrides them for that invocation. By this document's own argument that is the
more consistent choice rather than a lesser one, since a resolution that depends on what a
file said at the moment the command ran is the same class of thing the compiled-ahead
section objects to in generators. 66 keeps the escape hatch explicit and per-command
instead of ambient. The trade is real: no way to reconfigure the search set without
rebuilding.

**Instances deserve a separate note, because the two projects mean different things by the
word.** s6 has had them since 2.11.2.0 in January 2023, 66 since 0.2.1.0 in September 2019,
and s6's are *dynamic*: `s6-instance-maker` turns a template into a service directory that
is itself a nested supervision tree, and `s6-instance-create` spawns a copy at run time.
Instances come and go while the machine runs, without recompiling anything, which is
precisely the escape hatch a monolithic compiled database needs and the honest counterpart
to the ceremony criticised above. The cost is that individual instances live outside that
database: nothing can declare a dependency on one, because it did not exist when the graph
was compiled. 66's `@` is the other choice. An instance is materialised at parse time into
its own CDB and is an ordinary service in the graph, so it can be depended on, put in a tree
and enabled like any other, at the price of existing before it is needed. Neither is a
subset of the other.

Masking is the more interesting one, because it answers different questions on each
side. s6-frontend's own documentation gives its reason: masking is for when "the stores
provide services that come from random installed packages, the user never wants to run
these services, and it's just better not to see them". That is legibility, and it is needed
because the compiled database holds everything the stores hold: a consequence of one
database for all services rather than a capability. systemd's `mask` does something else and
harder. The unit is symlinked to `/dev/null` and becomes unloadable, so it cannot be started
*even when another unit depends on it*. That is a veto against activation the administrator
does not control: socket units, D-Bus activation, a `Wants=` shipped by a third-party
package, a generator.

66 has neither problem. One CDB per service means there is no crowded database to hide
things from. And activation stays enumerable: there is no socket activation and there are
no generators, while D-Bus activation reaches only the services `66-dbus-launch` translated
into the `dbus` tree, each of which exists as a frontend you can read and a CDB you can
inspect, and each of which stops being activatable when its bus service file goes away.
`66 disable` and the service does not come up. For the one residual case, a packaged
definition that someone else's dependency drags in and you do not want, the layering
already answers it: shadow the frontend in `/etc/66/service`, which is explicit and
reviewable where a symlink to `/dev/null` is neither. The veto is unnecessary because the
set of things that can start a service is small and named, not because nothing can.

**dinit is the nearest as a competitor**, and the split is clean:

- **dinit** wins on portability (Linux, FreeBSD, macOS, though see above: the gap is one
  missing kqueue backend in oblibs, not an architectural tie to Linux), on conceptual
  simplicity, and on adoption;
- **66** wins on administrative power (trees, modules, instances, snapshots, live
  migration, events) and on the robustness of the supervision model.

Put another way: runit is a tool, OpenRC is a script scheduler, finit is an embedded init,
the s6 stack is a supervision toolkit that has grown a friendly face, dinit is a good
general-purpose manager, systemd is a system platform, and **66 is a complete service
manager that refuses to become a platform.** That is a coherent position, not a compromise.

Which leaves one real question, and it is not "66 versus systemd", nor even "cgroups or
not". It is whether a service manager owes its administrator *resource ceilings*, a memory
cap and a CPU weight per service, the way it already owes them rlimits.

66 answers yes, and it is on the roadmap. Not as a change of position on cgroups as a
tracking substrate; that refusal stands, and the PID namespace already covers the job. But
a ceiling is an *attribute* of a service, in the same family as `LimitAS`, and a manager
that grants rlimits owes the ceiling too. It costs far less than it looks, because
declining cgroups for tracking is exactly what makes them cheap to adopt for resource
control. The work is priced out in
[*What cgroups and seccomp would cost 66*](https://git.obarun.org/Obarun/66/-/blob/master/what-cgroups-and-seccomp-would-cost.md), along
with seccomp, the other gap regularly raised here.

