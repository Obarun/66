# env

Publish per-session environment variables to every service of a scandir.

## Interface

```
env [ -h ] import|set|unset|list [<subcommand options>] variable...
```

A [scandir](66-scandir.html) is started long before a session exists — a user one at the first
login, the system one at boot. Its environment is frozen at that point and cannot be reloaded.
Variables that only a session knows — `DISPLAY`, `WAYLAND_DISPLAY`, `XAUTHORITY`, or anything
else a session wants to hand over — therefore never reach the services it supervises.

`env` is the way in. It writes to the *runtime environment directory* of the scandir owner,
`%%livedir%%/environment/<uid>/`, one file per variable, named after it. Every service picks up
what is published there the next time it starts, without declaring anything in its frontend.

The directory is created along with the scandir and destroyed with it, so nothing published
survives a `scandir remove`. `env` never creates it: if it is missing, the command says so
instead of publishing into a store no service reads.

## Subcommands

- **import** *variable...*: publish each *variable* with the value it has **in the environment
  of the caller**. This is pure transport — the value is copied as it is found. A variable that
  is not set, or set to an empty value, is skipped with a warning; nothing is computed, guessed
  or checked for reachability.

- **set** *variable=value...*: publish each pair with the given *value*.

- **unset** *variable...*: withdraw each *variable*. A variable that is not published is
  reported with a warning and the command carries on.

- **list**: print the published variables, one `variable=value` pair per line.

## Options

- **-h, --help**: prints this help.

## Precedence

`66-execute` rebuilds the environment of a service every time it starts. The runtime
environment is merged **last**, so a published variable wins over every other source:

```
    environment inherited from the scandir
 <  the [Environment] section of the frontend
 <  the service configuration file (66 configure)
 <  the ImportFile files
 <  %%livedir%%/environment/<uid>/                    <- 66 env
```

The whole stack is then expanded **once**, when every source has been merged. A published
value is therefore a first-class part of the environment rather than a late patch on it: a
`${...}` it contains is resolved like any other, and the variable it defines can itself be
referenced by a `${...}` written in the `Execute` field of a frontend — see
[Substitution in Execute](66-frontend.html#substitution-in-execute).

Two consequences worth keeping in mind:

* A `${HOME}` typed into a published value is **not** kept literal; it resolves against the
  environment the service ends up with. `env list` and the stored file still show it as it was
  typed — the expansion happens when a service starts, not when the value is published.
* A publication **replaces the whole declaration** of a key, its `!` marker included. A key
  that a frontend keeps out of the environment with `!` is exported again, with the published
  value, as soon as it is published here — the last source merged wins, as everywhere else.
  `env set` still refuses a value starting with an exclamation mark, so a publication can
  never introduce an unexported key itself.

## Events

Publishing and withdrawing raise **two distinct events** on the [event
daemon](66-eventd.html) of the scandir:

| Raised by | Event | Means |
|---|---|---|
| `import`, `set` | `env.<variable>` | *variable* is now published with a usable value |
| `unset` | `unenv.<variable>` | *variable* is no longer published |

Two names rather than one, because a reactor is woken by a name and nothing else: a single
`env.DISPLAY` for both facts would start a service on the very disappearance it was waiting
to avoid. A service subscribes to the one it needs:

```ini
# frontend: myterm
[Main]
Type = classic
Description = "a terminal that needs a display"
[Start]
Execute = ( xterm )
[Event]
EventType = user
On = ( env.DISPLAY )
Do = start
```

Reacting to both directions takes two services, since a reactor declares one `Do`: the one
above starts on `env.DISPLAY`, a second one subscribes to `unenv.DISPLAY` with `Do = stop`.

Like the other names 66 raises itself, both carry a dot, which sets them apart from the bare
names raised with [emit](66-emit.html). See [the event system](66-event.html) for how sources
and reactors fit together.

If the event daemon cannot be reached, the variable is still published — the command warns and
returns success, since the publication it announces has already taken place. Services will
pick the value up at their next start.

## Limits

A variable name must match `[A-Za-z_][A-Za-z0-9_]*`: it is both the name of the file holding
the variable and the tail of the event raised for it.

An environment directory cannot hold more than `20` files, so at most **20 variables** can be
published at a time. `env` refuses the twenty-first with an explicit error rather than letting
the limit break the next service start. See [Environment](66-scandir.html#environment) for the
syntax and the other limits of an environment directory.

## Exit codes

- *0* success
- *100* wrong usage
- *111* system call failed

## Usage examples

Hand the display over to the user services from an Xorg startup hook, the same way
`/etc/X11/xinit/xinitrc.d/` scripts do it for other service managers:

```
66 env import DISPLAY XAUTHORITY
```

Publish a value directly:

```
66 env set EDITOR=nvim
```

See what a session has published:

```
66 env list
```

Withdraw the display when the session ends:

```
66 env unset DISPLAY XAUTHORITY
```

## See also

- [scandir](66-scandir.html): the scandir owning the runtime environment directory.
- [configure](66-configure.html): the persistent, per-service environment.
- [the event system](66-event.html): reacting to a variable appearing or disappearing.
