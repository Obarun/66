# Cheatsheet

Every command you reach for day to day, on one page. New to `66`? Read the
[getting started](66-getting-started.html) tutorial first — this page is a quick
reference, not an introduction.

In the tables below, `foo` is a service name and `mytree` a tree name. Run `66`
as a regular user for your own services, or as root for system services; the
commands are the same.

Commands are shown with **long options** for readability. Every long option has
a short equivalent (for example `--start` is `-S`, `--tree` is `-t`), listed on
each command's reference page. An option that takes a value accepts both
`--depth 2` and `--depth=2`.

## Service life cycle

| Goal | Command |
| --- | --- |
| Run a service now | `66 start foo` |
| Register it for the next boot/session | `66 enable foo` |
| Register **and** run it now | `66 enable --start foo` |
| Stop it (stays enabled) | `66 stop foo` |
| Un-register it from boot | `66 disable foo` |
| Stop and unregister in one go | `66 disable --stop foo` |
| Restart (down then up) | `66 restart foo` |
| Reload config — sends `SIGHUP` | `66 reload foo` |
| Re-apply an edited frontend file | `66 reconfigure foo` |
| Stop and drop from the scandir | `66 free foo` |
| Erase everything `66` generated for it | `66 remove foo` |

`66 remove` is irreversible (it keeps only your frontend file). Several services
at once: separate names with spaces — `66 start foo bar baz`.

**Golden rule**: `start` = *now*; `enable` = *on every future boot/session*.

## Inspecting services

| Goal | Command |
| --- | --- |
| Status of one service | `66 status foo` |
| Overview of all services | `66 status` |
| Show dependencies as a graph | `66 status --graph foo` |
| Limit graph depth | `66 status --depth 2 foo` |
| Reverse (stop) order | `66 status --reverse foo` |
| Pick fields (scriptable) | `66 status --no-name --field name,status foo` |
| Compile a frontend without running it | `66 parse foo` |
| Dump the parsed resolve file (debug) | `66 resolve foo` |
| Dump the runtime state file (debug) | `66 state foo` |

## Supervision (scandir)

| Goal | Command |
| --- | --- |
| Create the supervision directory | `66 scandir create` |
| Start supervision (creates if needed, idempotent) | `66 scandir start` |
| Re-scan for new/removed services | `66 scandir reconfigure` |
| Force an immediate scan | `66 scandir check` |
| Clean shutdown of supervision | `66 scandir stop` |

`66 scandir start` returns at once if a scandir is already running; otherwise it
launches the supervisor in the **foreground** — background it (`&`) or use a
dedicated terminal. Normally `66-userd` (system: [66 boot](66-boot.html)) does
this for you, so you rarely type it.

## Trees

| Goal | Command |
| --- | --- |
| Create a tree | `66 tree create mytree` |
| Enable a tree (brought up at boot) | `66 tree enable mytree` |
| Make a tree the current one | `66 tree current mytree` |
| Show a tree's services | `66 tree status mytree` |
| Bring a tree up / down | `66 tree start mytree` / `66 tree stop mytree` |
| Remove a tree | `66 tree remove mytree` |
| Enable a service into a tree | `66 --tree mytree enable foo` |

Without a chosen tree, services go to the default tree `%%default_treename%%`.

## Configuration & environment

| Goal | Command |
| --- | --- |
| Edit a service's environment file | `66 configure foo` |
| List available config versions | `66 configure --versions foo` |
| Set the current config version | `66 configure --current version foo` |
| Override one `key=value` | `66 configure --replace 'KEY=value' foo` |

After changing config of a running service, apply it with `66 reconfigure foo`.

## Snapshots

| Goal | Command |
| --- | --- |
| Snapshot the whole 66 ecosystem | `66 snapshot create name` |
| Restore a snapshot | `66 snapshot restore name` |
| List snapshots | `66 snapshot list` |
| Delete a snapshot | `66 snapshot remove name` |

A snapshot only covers the ecosystem of the user who runs it.

## System control

| Goal | Command |
| --- | --- |
| Boot the system (PID 1) | `66 boot` |
| Power off | `66 poweroff` |
| Reboot | `66 reboot` |
| Halt | `66 halt` |
| Suspend to RAM | `66 suspend` |
| Hibernate to disk | `66 hibernate` |

## Minimal frontend file

The smallest valid service (see [frontend](66-frontend.html) for everything):

```
[Main]
Type = classic

[Start]
Execute = ( /usr/bin/mydaemon --foreground )
```

`Type` is `classic` (supervised, restarted), `oneshot` (runs once) or `module`
(a bundle).

## Common identifiers

Replaced at parse time, for generic/reusable files (see
[identifier](66-identifier.html)):

| Token | Replaced by |
| --- | --- |
| `@I` | Instance name (string after the `@`) |
| `@U` | User name (`root` for uid 0) |
| `@H` | User home directory |
| `@R` | User runtime directory (`/run` for root) |

## Key paths

| What | Regular user | Root / system |
| --- | --- | --- |
| Frontend files | `$HOME/%%service_user%%` | `%%service_adm%%`, `%%service_system%%` |
| Config files | `%%service_userconf%%` | `%%service_admconf%%` |
| 66 working dir | `%%user_dir%%` | `%%system_dir%%` |
| Live supervision | `%%livedir%%` | `%%livedir%%` |
