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

Aliases. The service can be referenced (as a dependency, or on the command line)
under any of these alternate names, like symbolic links.

```
Provide = ( network networking )
```

A service in any bracketed list can be **commented out** with a leading `#`
without removing the line:

```
Depends = ( database #cache network )
```

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
