# Dependencies and ordering

A real system is a web of services that need each other: a web app needs its
database, which needs the network, which needs the filesystem mounted. `66`
lets you declare only the **direct** relationships of each service; it works out
the full chain — in both directions — automatically.

This guide gathers the five relationship keys (scattered across the
[frontend reference](66-frontend.html)) and shows how ordering actually plays
out. You declare these keys in the `[Main]` section of a frontend file.

## You declare direct links, 66 resolves the chain

You never write the full ordered list of what to start. You state, per service,
what it directly needs — and `66` resolves the rest recursively, as a directed
acyclic graph (DAG). See [handling dependencies](66.html#handling-dependencies).

```
              start order  ───────────────▶
   filesystem ──▶ network ──▶ database ──▶ webapp
              ◀───────────────  stop order
```

- On **start**, a service's dependencies come up **first**: starting `webapp`
  brings up `database`, which brings up `network`, and so on.
- On **stop**, the order reverses: stopping `network` stops `database` first,
  then `webapp` — whoever needs it goes down before it does.

Both directions are recursive and handled by every command that needs it
(`start`, `stop`, `enable`, `disable`, `restart`…). Pass `--no-propagate`
(`-P`) to act on a single service without touching its chain: the named
services only are acted upon, in the order of their dependencies, and neither
their dependencies nor their logger are parsed, enabled, started or stopped by
it. It is a temporary action: the next command without `-P` runs its normal
flow, whatever `-P` left behind.

Between services that do not depend on each other, no order is guaranteed.

## The five relationship keys

All five take a bracketed list, and a service in such a list can be **commented
out** with a leading `#` without removing the line:

```
Depends = ( database #cache network )
```

### Depends — "I need these first"

The forward link. Each listed service must start successfully **before** this
one launches.

```
[Main]
Type = classic
Depends = ( database network )
```

### RequiredBy — "these need me"

The reverse link: it declares services that depend on this one. A listed
service depends on this one exactly as if its own frontend listed this one in
its `Depends`: stopping this one stops it first. A listed service that was never
parsed is not parsed by the declaration; it depends on this one once it is
parsed, by its name or through a `Depends`.

```
RequiredBy = ( webapp worker )
```

### OptsDepends — "one of these, or none"

An optional dependency list. At enable time, `66` walks the list and enables the
**first available** one, then stops looking. Order matters. If one is already
enabled, it warns and does nothing; if none is found, it warns and continues.

```
OptsDepends = ( dhcpcd connman networkmanager )
```

### Conflict — "we cannot coexist"

Services that must never run or be enabled at the same time as this one. If a
conflicting service is up, starting this one fails; if it is enabled, enabling
this one is rejected.

```
Conflict = ( connman networkmanager )
```

### Provide — "I also answer to these names"

Names the service answers to, in addition to its own. A name is a bridge
between what a frontend or a command line says and the service that currently
answers to it: a consumer writes `Depends = ( network )`, keeps that name in
its resolve, and reaches whoever provides `network` when the graph is built.

```
Provide = ( network networking )
```

**One name, one thing.** A name designates a service bearing it, or a provider
of it, never both at once. The service bearing a name keeps it while it is
enabled; disabled, an enabled provider may take the name over, and the service
stays on disk. Among providers, exactly one answers to the name at a time, and
who that is changes on these events only:

| event | who answers afterwards |
|---|---|
| a provider enters the system while the name is free, because you enable it or because a dependency pulls it in | that provider |
| you enable a provider while the provider answering is disabled | that provider |
| you enable a provider while the service bearing the name is disabled | that provider, the service stays on disk |
| a service bearing the name is parsed while the provider answering is disabled | the service, it takes its name back |
| you enable the service bearing the name while the provider answering is disabled | the service, it takes its name back |
| you remove the provider answering | nobody, until another provider enters the system or is enabled |
| you disable the provider answering | unchanged, it keeps answering |

The last line is what makes a dependency on the name behave like any other: a
disabled provider is started by whoever depends on it, exactly as a disabled
service would be.

> **Handing a name over.** To move a name from the enabled provider answering
> to it to another one, disable the first with `66 disable -P`, which leaves its
> consumers alone, then enable the other. The same holds between a service and a
> provider of its name. Removing a provider removes the services that depend on
> it with it, and releases the name: nobody answers to it afterwards.
>
> `66 disable -P` stops nothing: the service left keeps running until it is
> stopped or until the next boot. The full sequence, from the service `dbus` to
> the provider `dbus-launch`:
>
> ```
> 66 stop -P dbus dbus-log
> 66 disable -P dbus
> 66 enable dbus-launch
> ```
>
> `66 status dbus` then answers for `dbus-launch`, and the services that depend
> on `dbus` reach it. The service `dbus` stays on disk; `66 remove dbus`, which
> takes the name literally, removes it and its logger only, and leaves
> `dbus-launch` and its consumers in place. While it stays, the way back is:
>
> ```
> 66 stop -P dbus-launch dbus-launch-log
> 66 disable -P dbus-launch
> 66 enable dbus
> ```
>
> `66 enable dbus` takes the name back from the disabled `dbus-launch`.

**What is refused.** Each refusal below happens before anything is written, so
a refused command leaves the system as it was:

| you ask for | refused when | way out |
|---|---|---|
| `66 enable` of a provider | the enabled service bears the name it provides | `66 disable -P` that service |
| `66 enable` of a provider | another provider answering to that name is enabled | `66 disable -P` that provider |
| `66 enable` of a service | an enabled provider answers to the name the service bears | `66 disable -P` that provider |
| `66 enable` of a provided name | a command line names services, never providers | enable the provider by its own name |
| `66 enable` of a consumer | it pulls in both a service and a provider of that service's name | drop one of the two frontends |
| `66 enable` of a consumer | nobody answers to a name it depends on | enable a provider of that name first |

The system reads a frontend when it needs one, and `66 enable` does that for
you: these refusals reach you through it. An administrator calling
[66 parse](66-parse.html) by hand meets the same ones, worded the same way.

**Names on the command line.** Every command that acts on a running service, or
asks about one, follows the name: `66 status network` answers for the service
providing it. `66 remove` acts on a frontend and takes the name literally:
`66 remove network` removes a service called `network`, never its provider.

## A worked example

Three frontend files:

```
# file: network
[Main]
Type = oneshot
[Start]
Execute = ( echo "network up" )

# file: database
[Main]
Type = classic
Depends = ( network )
[Start]
Execute = ( /usr/bin/mydb --foreground )

# file: webapp
[Main]
Type = classic
Depends = ( database )
[Start]
Execute = ( /usr/bin/myapp --foreground )
```

Now a single command:

```
66 start webapp
```

`66` parses `webapp`, sees it needs `database`, which needs `network`, and
brings them up in that order: `network` → `database` → `webapp`. Later,
`66 stop network` tears the whole stack down in reverse: `webapp` → `database`
→ `network`.

## Inspecting the graph

| Goal | Command |
| --- | --- |
| See a service's dependency graph (start order) | `66 status --graph webapp` |
| See it in reverse (stop order) | `66 status --reverse webapp` |
| Limit how deep the graph is drawn | `66 status --depth 2 webapp` |
| Overview of every service and tree | `66 status` |

## What is written, what is computed

The resolve file of a service holds what its frontend declares: its `Depends`,
the dependency `OptsDepends` found, its logger, and its own `RequiredBy`. What
a service is required by is never written in its resolve file: `66` computes it
when it builds the graph of the system, from every service's declarations.
`66 status -f requiredby` shows the result.

The commands read that graph from a file under
`%%livedir%%/graph/<uid>/system`, one per account, that `66` builds again after
each command that writes the system, at the start of the scandir and after a
reboot. The commands that write the system, the parse on the fly included, hold
an exclusive lock of the system while they write on disk, never while they act
on the supervision: a second writing command waits for the first one, and says
so at verbosity 3 once the wait lasts more than a second. A command that only
reads the system never waits.

## Absent and invalid services

A service is **absent** when a parsed service names it while it was never
parsed, or is no longer: a `Depends` of a service started under `-P`, a
`RequiredBy` naming a service never parsed, a `From` of a reactor, a removed
service still named in the `RequiredBy` of a neighbour. A command that prepares
(`start`, `enable`, `restart`, `reconfigure`, `status`) parses it when it reaches
it through a `Depends` or a `From`; a `RequiredBy` is never followed by a parse.
`stop`, `free`, `disable` and `remove` pass over it, while `signal` and `reload`,
which prepare nothing, refuse it.

A service is **invalid** when its resolve file or one of its addons cannot be
read, or when it is part of a dependency cycle or depends on one. A command
whose action touches it refuses with `100` and moves nothing; a command whose
action does not touch it acts, and warns at verbosity 2. The message names the
repair:

```
service: b has an unreadable resolve file, and is required by: c -- repair it with 66 reconfigure -c b
service: c cannot be ordered: it is part of, or depends on, a dependency cycle -- break the cycle in the frontends, then re-parse them with 66 parse -f
```

A service whose frontend is gone from every directory `66` looks in is not
invalid: the commands act from its resolve file. `66 parse -f`,
`66 reconfigure` and `66 status` naming it refuse it with `100`:

```
service: b has no frontend anywhere any more -- remove it with 66 remove b
```

A dependency cycle in the frontends is refused by the parse before anything is
written.

## Where to go next

- [frontend service file](66-frontend.html) — the full `[Main]` key reference.
- [tree](66-tree.html) — group services and order whole trees against each other.
- [66](66.html#handling-dependencies) — the propagation rules in detail.
