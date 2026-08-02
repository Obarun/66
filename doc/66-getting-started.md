# Getting started

This is a hands-on tutorial: starting from nothing, you will write, run,
supervise and clean up your first services with `66`. Follow it top to bottom
once — afterwards the [cheatsheet](66-cheatsheet.html) and the reference pages
will make a lot more sense.

The main walkthrough is written for a **regular user** (no root, no privileges
needed). Each step has a short **As root / system admin** note pointing out what
changes when you manage system services instead.

**Prerequisite**: `66` is installed on your machine. That is all.

## The four concepts you need

You only need a rough idea of these to follow along. Each links to its reference
page for later.

- **Frontend file** — a short `INI` file that *declares* a service (its type,
  what to execute, its dependencies…). You write these. See
  [frontend service file](66-frontend.html).
- **Scandir** — the live supervision tree (a set of [66-supervise](66-supervise.html) processes)
  that actually keeps your services running. It must be running before a service
  can run. See [scandir](66-scandir.html).
- **The pipeline** — `66` turns a frontend file into a running service in three
  moves: **parse** (compile the file), **enable** (register it for the next
  boot/session) and **start** (bring it up now). You rarely call `parse`
  yourself: `start` and `enable` do it for you.
- **Tree** — a named group of services you can manage together. If you do not
  pick one, services land in the default tree `%%default_treename%%`. See
  [tree](66-tree.html).

## Step 1 — Make sure a scandir is running

A service can only run inside a **scandir** — the live supervision tree
([66-scandir](66-scandir.html) and the
`66-supervise` processes under it) for your user. Whether one is already running
depends on how your system was set up, so it is worth knowing where it comes
from:

- A per-user scandir is normally brought up at boot by `66-userd`, the daemon
  that starts and supervises the scandir of each user. On a machine installed
  and booted the usual way — the Obarun ISO, for instance — your scandir is
  therefore **already running**.
- But you can boot with 66 and still have **no** scandir, if `66-userd` does not
  run on that system.
- And you can be on a machine that did **not** boot with 66 at all and still use
  66 — you just bring a scandir up yourself.

One command covers every case, because it is idempotent:

```
66 scandir start &
```

- If a scandir is already running for you, it **returns immediately** — no harm,
  nothing duplicated. (It also creates the scandir first if it does not exist.)
- If none is running, it launches `66-scandir` and **stays in the foreground**:
  that process *is* your supervisor from now on. In a normal boot this is exactly
  what `66-userd` does for you, under supervision — which is why you do not
  usually type it. When you bring one up by hand (a from-zero or non-66
  system), run it in a dedicated terminal and leave it there, or start it in the
  background with `&`.

This tutorial starts from zero, so go ahead and run it.

> **As root / system admin** — the system scandir is created and started by
> [66 boot](66-boot.html) as PID 1; you never start it by hand.

## Step 2 — Your first service (a oneshot)

Let's see the whole pipeline work in two minutes with the simplest possible
service. A **oneshot** runs once and does not stay up — perfect for a first
contact.

Create the file `$HOME/%%service_user%%/hello` with this content:

```
[Main]
Type = oneshot
Description = "my first service"

[Start]
Execute = ( echo "hello from 66" )
```

Now bring it up:

```
66 start hello
```

You never parsed it — `start` noticed and parsed it for you. Check what
happened:

```
66 status hello
```

The status shows the service, its type, the tree it landed in
(`%%default_treename%%`) and the last lines of its log, where you will find your
`hello from 66`.

> **As root / system admin** — system frontend files live in
> `%%service_adm%%` (your own) and `%%service_system%%` (shipped by packages)
> instead of `$HOME/%%service_user%%`. Everything else is identical.

## Step 3 — A supervised daemon (a classic)

Oneshots run and exit. A **classic** service is a long-running process that `66`
keeps alive and restarts if it dies. This is what supervision is really about.

Create `$HOME/%%service_user%%/heartbeat`:

```
[Main]
Type = classic
Description = "heartbeat demo daemon"

[Start]
Execute = ( loopwhilex foreground { echo "heartbeat" } sleep 5 )
```

The `Execute` field is an [execline](https://skarnet.org/software/execline)
script that prints a line every five seconds and loops forever. In a real
service you would replace it with your actual daemon command, for example
`Execute = ( /usr/bin/mydaemon --foreground )`. Start it:

```
66 start heartbeat
```

Check it:

```
66 status heartbeat
```

This time the service stays **up**: the status reports its `pid`, its uptime,
and the most recent log lines. Because a logger is created by default (the `log`
option in [[Main]](66-frontend.html#options) is implicit), every line the daemon
prints is captured and timestamped. Run `66 status heartbeat` again a few
seconds later: the uptime grows and new `heartbeat` lines appear.

To prove the supervision, kill the daemon's process by its `pid`: `66` brings it
straight back up — a fresh `pid`, the uptime reset to zero.

### execline is not mandatory

By default the `Execute` field is [execline](https://skarnet.org/software/execline).
But you are not tied to it: make the **first line** of `Execute` your own shebang
(`#!…`) and `66` detects it, then runs the script verbatim, in whatever language
you like.

The same heartbeat daemon in bash:

```
[Main]
Type = classic
Description = "heartbeat in bash"

[Start]
Execute = (#!/usr/bin/bash
while true ; do
    echo heartbeat
    sleep 5
done
)
```

…and in Python:

```
[Main]
Type = classic
Description = "heartbeat in python"

[Start]
Execute = (#!/usr/bin/python3
import time
while True:
    print("heartbeat", flush=True)
    time.sleep(5)
)
```

Two things to respect in a custom script: the shebang must be the **first line**
of `Execute` (66 strips any whitespace before it), and the program must stay in
the **foreground** — the `while` loops above never exit, so supervision keeps
working. (Note `flush=True` in Python: without it, stdout is buffered and your
logs would appear only in bursts.) See
[The Execute key in depth](66-frontend.html#appendix-b-the-execute-key-in-depth)
for the details.

## Step 4 — Make it persistent with enable

So far you only used `start`, which means *run it now*. It does **not** survive a
reboot or a new session. To have a service come back automatically, you
**enable** it:

```
66 enable --start heartbeat
```

The `--start` option (`-S` for short) enables **and** starts it in one go (here
it is already up, so it is a no-op). The golden rule:

- **`start`** = bring it up *now*.
- **`enable`** = bring it up *on every future boot / session*.

Most of the time you want both, which is exactly what `enable --start` gives you.

> **As root / system admin** — `enable` registers the service in a
> [tree](66-tree.html); pick one with `66 --tree mytree enable …` or the
> `InTree` key in the frontend file. The chosen tree is what gets brought up at
> boot.

## Step 5 — Stop and clean up

Walking back out, from the gentlest action to the most destructive:

```
66 stop heartbeat        # bring it down, keep it enabled
66 disable heartbeat     # no longer start it on boot/session
66 free heartbeat        # bring it down and drop it from the scandir
66 remove heartbeat      # erase everything 66 generated for it
```

`66 remove` cannot be undone — it deletes the parsed service and its state (it
leaves your frontend file alone). When you are done experimenting, you can bring
the whole supervision down too:

```
66 scandir stop
```

This is a clean shutdown: services are stopped and loggers flush before exiting,
so no log line is lost.

## Where to go next

- [Cheatsheet](66-cheatsheet.html) — every command you will use day to day, on
  one page.
- [Frontend service file](66-frontend.html) — all the sections and keys you can
  put in a service file.
- [Identifier interpretation](66-identifier.html) — write generic, reusable
  frontend files with `@I`, `@U`, `@H`…
- [tree](66-tree.html) — group and manage services together.
- [Module service usage](66-module-usage.html) — use ready-made bundles of
  services.
