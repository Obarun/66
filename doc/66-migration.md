# Coming from systemd, OpenRC or runit

If you already run services on another init or service manager, most of your
mental model carries over — only the spelling changes. This page maps the
commands and concepts you know to their `66` equivalents.

For migrating between `66` versions, see [upgrade](66-upgrade.html) and the
[Rosetta stone](66-rosetta.html) instead.

## Commands

| You want to… | systemd | OpenRC | runit | 66 |
| --- | --- | --- | --- | --- |
| Start now | `systemctl start foo` | `rc-service foo start` | `sv up foo` | `66 start foo` |
| Stop now | `systemctl stop foo` | `rc-service foo stop` | `sv down foo` | `66 stop foo` |
| Enable at boot | `systemctl enable foo` | `rc-update add foo` | `ln -s …` | `66 enable foo` |
| Disable at boot | `systemctl disable foo` | `rc-update del foo` | `rm …` | `66 disable foo` |
| Enable **and** start | `systemctl enable --now foo` | — | — | `66 enable --start foo` |
| Restart | `systemctl restart foo` | `rc-service foo restart` | `sv restart foo` | `66 restart foo` |
| Reload (SIGHUP) | `systemctl reload foo` | `rc-service foo reload` | `sv reload foo` | `66 reload foo` |
| Status & recent log | `systemctl status foo` | `rc-service foo status` | `sv status foo` | `66 status foo` |
| Read the log | `journalctl -u foo` | log file | log file | `66 log foo` |
| Follow the log | `journalctl -fu foo` | `tail -f` | `tail -f` | `66 log --follow foo` |
| Apply an edited unit | `systemctl daemon-reload` | — | — | `66 reconfigure foo` |

## Concepts

| Concept | systemd | OpenRC | runit | 66 |
| --- | --- | --- | --- | --- |
| Service definition | `foo.service` unit | `/etc/init.d/foo` script | `/etc/sv/foo/run` | `foo` [frontend file](66-frontend.html) (INI) |
| Long-running daemon | `Type=simple`/`notify` | `supervisor`/script | the norm | `Type = classic` |
| Run-once task | `Type=oneshot` | script, no daemon | — | `Type = oneshot` |
| Readiness notification | `Type=notify` | — | `./check` | `Notify = <fd>` |
| Group / target | `.target` | runlevel | `runsvdir` dir | [tree](66-tree.html) |
| Drop privileges | `User=` | — | `chpst -u` | `RunAs = user` |
| Resource limits | `Limit*=` | `rc_ulimit` | `softlimit` | `[Execute]` `Limit*` keys |
| Environment | `Environment=` / `EnvironmentFile=` | `conf.d` | `./env` | `[Environment]` section |
| Session variables handed to every service | manager-level environment command | — | — | [`66 env`](66-env.html) |
| React to a file change / a schedule / another service | separate trigger unit | — | — | [`[Event]`](66-event.html) section, in the reacting service itself |

## 66-only commands

These have no direct counterpart elsewhere — they fall out of 66's
parse-then-supervise model and its tree/snapshot features. Coming from another
init, these are the genuinely new tools in your hand:

| Command | What it does | Closest elsewhere |
| --- | --- | --- |
| [`66 parse`](66-parse.html) | Compile a frontend file into 66's internal form, without starting anything | — (units/scripts are read directly) |
| [`66 free`](66-free.html) | Stop a service **and** drop it from the live scandir (unsupervise), keeping it parsed and enabled | — (stopping never unsupervises) |
| [`66 remove`](66-remove.html) | Erase everything 66 generated for the service (parsed form + state); your frontend file is kept | manual `rm` of the unit + `systemctl daemon-reload` |
| [`66 reconfigure`](66-reconfigure.html) | Stop, unsupervise, re-parse and restart in one step, to apply an edited frontend | partial: `systemctl daemon-reload` then restart |
| [`66 scandir`](66-scandir.html) | Create / start / stop your own supervision tree (`66-scandir`) | — (the supervision root is PID 1, not user-managed) |
| [`66 tree`](66-tree.html) | Create and manage named **groups** of services as first-class objects, with their own dependencies | systemd `.target` (static config, not a managed object) |
| [`66 snapshot`](66-snapshot.html) | Capture, restore or transfer the **whole** 66 ecosystem (e.g. to clone it onto another machine) | — |
| [`66 resolve`](66-resolve.html) | Print the **complete** service as the system resolved it — every field, the generated run/finish scripts, the resolved on-disk and live paths | OpenRC/runit: none; systemd `systemctl show` is nearest but reports runtime properties, not the compiled definition |
| [`66 state`](66-state.html) | Dump a service's runtime state flags (parsed, supervised, up…), for debugging | — |
| [`66 configure`](66-configure.html) | Edit a service's **versioned** environment configuration | partial: drop-ins / `EnvironmentFile` (not versioned) |

**Built for scripting.** `66 resolve` is the authoritative view of a service —
exactly what `66` itself acts on — and, like every `66` command, it is made to be
parsed by scripts. Field selectors give you raw, label-free values:
`66 resolve -n -f run foo` prints just the generated run script,
`66 status -n -f pid,status foo` prints exactly those two columns. (`-f` picks
fields, `-n` drops the labels.) OpenRC and runit offer no structured service
introspection at all; systemd's `systemctl show` is the closest, but it exposes
the manager's runtime properties rather than the service's full compiled form the
way `resolve` does.

## A few differences worth knowing

**Daemons must run in the foreground.** Like any process supervisor, `66`
supervises the process it launches. The first choice is always to pass the
"don't fork" flag (`--foreground`, `-d`, `--nofork`…). See
[troubleshooting](66-troubleshooting.html).

When a daemon offers no such flag — the systemd `Type=forking` case — the
`66-ns` tool (from the `66-tools` package) supervises it anyway by giving it a
private PID namespace, inside which `66-ns` becomes a transparent pid-1 proxy
that follows the daemon across its forks. Wrap the command in `[Start]`:

```
[Start]
Execute = ( 66-ns -o unshare=pid dhcpcd )
```

See the [66-ns documentation](https://docs.obarun.org/66-tools/66-ns.html) for
the details, including the optional `--pidfile` for daemons that keep a sibling
process alive.

**Ordering and requirement are one relation, in two directions.** systemd splits
ordering (`After=`) from requirement (`Requires=`). `66` folds both into a single
relation: a dependency is started *first* **and** is required. You can state it
from either end — `Depends = ( db )` ("I need `db`") or, from the other side,
`RequiredBy = ( webapp )` ("`webapp` needs me"), the reverse link that lets a
service attach itself to others without editing their files (systemd's
`RequiredBy=`/`WantedBy=`). You declare only direct links; `66` resolves the whole
chain in both directions. See
[dependencies and ordering](66-dependencies.html).

**A session can hand variables to services after the fact.** A
[scandir](66-scandir.html) starts long before a session exists, so its
environment is frozen without `DISPLAY`, `WAYLAND_DISPLAY` or `XAUTHORITY`.
[`66 env`](66-env.html) is the way in: `66 env import DISPLAY XAUTHORITY`
copies the variables from the caller's environment, `66 env set KEY=value`
publishes an explicit value, `66 env unset` withdraws one, `66 env list` shows
what is published. Every service of that scandir picks it up at its next start,
without declaring anything in its frontend, and the published value is merged
**last** — it wins over the frontend, the service configuration and the
`ImportFile` files.

Where 66 goes further: publishing or withdrawing a variable also raises an event
(`env.DISPLAY`, `unenv.DISPLAY`), so a service can be *started by the arrival* of
the value it needs and stopped by its disappearance, rather than being restarted
by hand afterwards. See [the event system](66-event.html). Nothing published
survives the scandir; there is no persistent manager environment to clean up.

**Reacting to an event is declared in the service that reacts, not next to it.**
"Restart `foo` when this file changes" is usually expressed with a second file
that names `foo` as the thing to activate, so the rule lives outside `foo` and
`foo` never mentions it. 66 reverses the direction: `foo` carries an
[`[Event]`](66-event.html) section saying what happens **to itself**, and a
reaction has no target key at all — `66-eventd` can only run the verb on the
service that declared it.

The practical difference shows up when you maintain a system rather than write
one. Everything that can restart `foo` at 3 a.m. is written in `foo`'s own
frontend, so you read one file instead of searching the whole service tree. And
subscribing a service to an existing event changes exactly one file — its own —
never a file belonging to a service you do not own. The only key that reaches
outward, [`Emit`](66-event.html#emit), raises a *name*, never a command: whoever
cares subscribes on their side. See [the event system](66-event.html).

**Applying config changes is per-service, not global.** `systemctl daemon-reload`
re-reads *every* unit file for the whole manager at once — a global operation
that, on its own, restarts nothing. [`66 reconfigure foo`](66-reconfigure.html)
is the opposite: it targets *one* service (and its dependency chain), re-parsing
its frontend and bringing it back up. There is no "reload everything" switch in
`66` — you apply changes service by service, or a whole [tree](66-tree.html) at a
time. So the `daemon-reload` ↔ `reconfigure` row above is a rough mapping, not an
exact equivalence.

## A unit, translated

A typical systemd unit:

```ini
[Unit]
Description=My daemon
After=network.target
Requires=network.target

[Service]
Type=simple
ExecStart=/usr/bin/mydaemon --foreground
User=myuser

[Install]
WantedBy=multi-user.target
```

becomes, as a `66` frontend file named `mydaemon`:

```
[Main]
Type = classic
Description = "My daemon"
Depends = ( network )

[Start]
RunAs = myuser
Execute = ( /usr/bin/mydaemon --foreground )
```

and you place it in `%%service_adm%%`, then `66 enable --start mydaemon`. The
`WantedBy` target maps to enabling it (optionally into a named
[tree](66-tree.html)).

## Where to go next

- [Getting started](66-getting-started.html) — build and run your first service.
- [frontend service file](66-frontend.html) — the full key reference.
- [Cheatsheet](66-cheatsheet.html) — the command quick reference.
