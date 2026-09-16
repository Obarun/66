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
(`-P`) to act on a single service without touching its chain.

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

The reverse link: it declares services that depend on this one. Enabling this
service updates those listed; stopping it stops them first.

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
of it, never both. Among providers, exactly one answers to the name at a time,
and who that is changes on these events only:

| event | who answers afterwards |
|---|---|
| a provider enters the system while the name is free, because you enable it or because a dependency pulls it in | that provider |
| you enable a provider | that provider, whoever answered before |
| you remove the provider answering | nobody, until another provider enters the system or is enabled |
| you disable the provider answering | unchanged, it keeps answering |

The last line is what makes a dependency on the name behave like any other: a
disabled provider is started by whoever depends on it, exactly as a disabled
service would be.

> **Removing a provider with `-P`.** `66 remove -P` keeps the services that
> depend on the one it removes, which is what you want to reinstall a provider
> without tearing down its consumers. The name is released all the same, and
> nothing bears it: until another provider answers to it, every command that
> builds the system graph fails on the orphaned name, `unable to find service
> frontend file of: network`, including commands that have nothing to do with it.
> Hand the name over in the same breath, by enabling another provider, or drop
> the `-P` and let the consumers go with it.

**What is refused.** Each refusal below happens before anything is written, so
a refused command leaves the system as it was:

| you ask for | refused when | way out |
|---|---|---|
| `66 enable` of a provider | a service bears the name it provides | `66 remove` that service |
| `66 enable` of a provider | another provider answering to that name is enabled | `66 disable` that provider |
| `66 enable` of a service | a provider answers to the name the service bears | `66 remove` that provider |
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

## Where to go next

- [frontend service file](66-frontend.html) — the full `[Main]` key reference.
- [tree](66-tree.html) — group services and order whole trees against each other.
- [66](66.html#handling-dependencies) — the propagation rules in detail.
