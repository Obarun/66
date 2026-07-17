# Running 66 inside a container

This is a hands-on walkthrough. It takes you from an empty image to a container
whose **pid 1 is 66**, supervising a real service tree, that you can drive from
the outside and stop with a chosen exit status. The same red thread is followed
twice: once on Obarun's own base image, once on a generic distribution
(Alpine), so you can see exactly what changes and what does not.

Everything shown here has been run end to end with Docker; the commands are
reproducible as written.

## Two ways to use 66 in a container

There are two distinct scenarios, and this page is about the second one:

1. **66 as a plain service manager**, under whatever init the container already
   has. You install 66, create a [scandir](66-scandir.html) and supervise a few
   services inside it. This is no different from using 66 on a normal host and
   is not covered here.

2. **66 as the container's init (pid 1).** 66 boots the container the same way it
   boots a machine — it brings up the enabled [trees](66-tree.html), keeps every
   service supervised, and stays pid 1 for the lifetime of the container. This is
   what [66 boot -c](66-boot.html) is for, and it is the subject of this page.

## What 66 actually needs (and what it does not)

A common assumption is that 66 needs the skarnet packages (`skalibs`,
`execline`, `s6`) installed on the system. That is **not** the case, and knowing
this is what makes the Alpine path straightforward.

- **To build 66** you need a C toolchain and [oblibs](https://git.obarun.org/obarun/oblibs).
  oblibs is self-contained (it needs only the C library), so **none** of the
  skarnet `-dev` packages are required. 66 links against `liboblibs` and the C
  library only — you can confirm this with `ldd` on the installed binary.

- **At run time** 66 needs the [execline](https://skarnet.org/software/execline)
  binaries, and nothing else from skarnet. The `init` skeleton and the service
  run scripts are execline scripts, so `execlineb` and its companions must be on
  the `PATH`. 66's supervision is **native** (its own
  [66-supervise](66-supervise.html), [66-scandir](66-scandir.html) and
  [66-log](66-log.html)): no `s6` binary is spawned at run time.

The practical consequence for Alpine: the skarnet packages it ships are used
only to pull in `execline` at run time. **Their versions are irrelevant to 66** —
66 is built from source against oblibs regardless. Any current Alpine release
works.

## How a container boot differs from a hardware boot

[66 boot -c](66-boot.html) follows the *same* path as a hardware boot — it
creates the live [scandir](66-scandir.html), starts every tree of the **boot
group** and then every enabled tree — with two container-specific differences:

- **No hardware preparation.** 66 does not mount a `devtmpfs` and does not trap
  ctrl-alt-del; the container runtime already provides `/dev`. 66 still mounts a
  `tmpfs` on the basename of its live directory (`%%livedir%%` → `/run`), which
  is why the container needs the means to perform that one mount (see
  [Running the container](#running-the-container)).

- **Leaving is an exit, not a reboot.** On real hardware the machine is handed
  back to the kernel. In a container, pid 1 simply **exits with a status code**.
  [66 halt](66-halt.html) makes it exit with the code held in the halt file
  (`EXITCODE`, default `0`); if the boot itself fails, pid 1 exits with `111`.

## What you provide, and what 66 sets up itself

66 keeps its own state — its system directories, a version marker, the default
enabled `global` [tree](66-tree.html) — under `/var/lib/66`. On a normal machine
your distribution's package creates that state at install time. **In a container,
66 creates it itself on the first boot**, so an image built from source needs
nothing baked in.

That setup is **best-effort**: it needs `/var/lib` to be writable. In a plain
`docker run` it is (the container's writable layer) and the boot sets everything
up. If you mount `/var/lib` read-only, see
[Read-only /var/lib](#read-only-varlib) below — the behaviour there is not
ambiguous, and it is spelled out.

So the only thing that must come from you is the **logger user**: 66 cannot
create a system account for itself.

### The logger user (recommended hardening)

The `catch-all` logger and every per-service logger run as the `-D 66-log-user`
compiled into 66, which is **root** by default — so an image that creates no user
at all already works. Running them as an unprivileged user is a simple,
worthwhile hardening, and is what the shipped Dockerfiles do: build 66 with
`-D 66-log-user=66log` and create that user in the image:

```
groupadd -r 66log && useradd -r -g 66log -s /usr/bin/nologin -d /dev/null 66log
```

If you name a `-D 66-log-user` that does not exist in the image, the first 66
command aborts while chowning the log directory.

That is the whole bootstrap. The container then boots — an empty but fully
supervised system. You add services next.

### Read-only /var/lib

Mounting `/var/lib` read-only is supported, but then 66 cannot create its state
and **you must bake it into the image**. The three cases, exactly:

| `/var/lib` at run time | State in the image | Result |
|---|---|---|
| writable (the default) | none needed | 66 sets everything up on the first boot |
| read-only | baked at build | boots normally, 66 only reads the state |
| read-only | none | **boot fails**: `unable to create directory: /var/lib/66/system: Read-only file system`, pid 1 exits `111` |

To bake the state at build time — where the filesystem is writable — run any 66
command other than `boot` in the Dockerfile. `66 status` is a natural choice: it
creates the state and confirms the install works:

```dockerfile
RUN 66 status
```

### Where services run: the boot group and the enabled trees

At boot, 66 brings services up in two waves:

1. Every tree that belongs to the **boot group** — regardless of its name. This
   is where you put services that must be up before anything else.
2. Every **enabled** tree, including the default `global` tree.

So there are two equally valid places to put a service, and neither needs any
`init.conf` change:

- **Enable it in the `global` tree** (the simplest: `66 enable myservice`). It
  comes up in the second wave. This is enough for most containers.
- **Create a boot-group tree** for early services:
  `66 tree create -o groups=boot core` then `66 -t core enable myservice`. Its
  name is free — the *group* is what marks it for the boot wave.

> A boot-group tree cannot be *enabled* (that is reserved for the second wave), so
> the two waves never start the same tree twice. If no tree is in the boot group,
> the first wave is simply a no-op.
>
> Pinning a single tree by name is still possible: set `TREE=<name>` in
> [init.conf](66-boot.html) (or on the kernel command line) and the first wave
> starts exactly that tree instead of the boot group.

## Path A — the Obarun base image

Obarun is 66's home distribution: `obarun/base` **already ships 66 and its
unprivileged logger user**. There is nothing to install and nothing to create —
the only difference from the base image is the `CMD`, which boots the container
instead of dropping you in a shell. The whole
[`contributions/docker/dockerfile`](https://git.obarun.org/Obarun/66/-/blob/dev/contributions/docker/dockerfile)
is:

```dockerfile
FROM obarun/base

CMD ["/usr/bin/66", "boot", "-c"]
```

Build and run it:

```
docker build -f contributions/docker/dockerfile -t 66-container .
docker run -d --name my66 --cap-add SYS_ADMIN 66-container
```

> This needs a base image whose packaged 66 creates its own state on the first
> boot — 66 `0.9.0.0` and later. On an older base, `66 boot` still expects a
> state that a build step prepared for it.

Building 66 from source on top of `obarun/base` only makes sense when you are
*developing 66 itself*; that is what
[`dockerfile.dev`](https://git.obarun.org/Obarun/66/-/blob/dev/contributions/docker/dockerfile.dev)
is for — see [Developing 66 in a container](#developing-66-in-a-container).

## Path B — a generic distribution (Alpine)

Alpine does not package oblibs, 66 or 66-tools, so we build all three from
source. As explained above, we do **not** need the skarnet `-dev` packages; we
only pull `execline` from `apk` for the run time. The repository ships this as
[`dockerfile.alpine`](https://git.obarun.org/Obarun/66/-/blob/dev/contributions/docker/dockerfile.alpine):

```dockerfile
FROM alpine:3.22

# Build toolchain + execline (the only skarnet runtime dependency).
RUN apk add --no-cache build-base meson ninja pkgconf git linux-headers execline

RUN git clone -b dev https://git.obarun.org/obarun/oblibs.git \
 && meson setup oblibs/builddir oblibs --prefix=/usr --libdir=lib \
 && meson compile -C oblibs/builddir \
 && meson install -C oblibs/builddir

RUN git clone -b dev https://git.obarun.org/obarun/66.git \
 && meson setup 66/builddir 66 --prefix=/usr --libdir=lib \
      -D 66-log-user=66log -D 66-log-timestamp=iso \
 && meson compile -C 66/builddir \
 && meson install -C 66/builddir

RUN git clone -b dev https://git.obarun.org/obarun/66-tools.git \
 && meson setup 66-tools/builddir 66-tools --prefix=/usr --libdir=lib \
 && meson compile -C 66-tools/builddir \
 && meson install -C 66-tools/builddir

# The unprivileged logger user (busybox addgroup/adduser here).
RUN addgroup -S 66log \
 && adduser -S -D -H -G 66log -s /sbin/nologin 66log

CMD ["/usr/bin/66", "boot", "-c"]
```

Build and run it exactly like the Obarun image:

```
docker build -f contributions/docker/dockerfile.alpine -t 66-alpine .
docker run -d --name my66 --cap-add SYS_ADMIN 66-alpine
```

Compared to Path A, you build the three projects yourself and create the logger
user; the boot itself is identical — that is the whole point.

## Path C — Debian

Debian is the same story as Alpine — it packages `execline` but not oblibs/66/66-tools —
with **one distribution-specific twist you must not miss**. Debian keeps the
execline commands out of `/usr/bin`, where their generic names (`fdmove`,
`redirfd`, `if`, …) would clash with other packages, and ships them in
`/usr/lib/execline/bin`; only `execlineb` itself lands in `/usr/bin`.

66 bakes those paths into the scripts it generates, so the build must be told:

```dockerfile
RUN git clone -b dev https://git.obarun.org/obarun/66.git \
 && meson setup 66/builddir 66 --prefix=/usr --libdir=lib \
      -D execline-bindir=/usr/lib/execline/bin \
      -D 66-log-user=66log -D 66-log-timestamp=iso \
 && meson compile -C 66/builddir \
 && meson install -C 66/builddir
```

The default `-D shebangdir` (`/usr/bin`) is already right, since that is where
Debian puts `execlineb`. **Without `-D execline-bindir`** the generated run
scripts call a `/usr/bin/fdmove` that does not exist: the `catch-all` logger
never starts, and the boot hangs waiting for it — a live but incomplete
container, with no log to tell you why.

The rest is ordinary Debian packaging. The full file is
[`dockerfile.debian`](https://git.obarun.org/Obarun/66/-/blob/dev/contributions/docker/dockerfile.debian):

```dockerfile
FROM debian:stable

# Build toolchain + execline (the only skarnet runtime dependency).
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential meson ninja-build pkg-config git ca-certificates execline \
 && rm -rf /var/lib/apt/lists/*

# ... oblibs, 66 (with -D execline-bindir above) and 66-tools from source ...

# The unprivileged logger user.
RUN groupadd -r 66log \
 && useradd -r -g 66log -s /usr/sbin/nologin -d /dev/null 66log

CMD ["/usr/bin/66", "boot", "-c"]
```

```
docker build -f contributions/docker/dockerfile.debian -t 66-debian .
docker run -d --name my66 --cap-add SYS_ADMIN 66-debian
```

The lesson generalises: if a distribution relocates the execline commands, point
`-D execline-bindir` at them. You can check with `dpkg -L execline` (or your
package manager's equivalent).

## Developing 66 in a container

The Dockerfiles above clone 66 from git. When you are working *on* 66 itself that
is exactly what you do not want — your uncommitted changes are not there yet.
[`dockerfile.dev`](https://git.obarun.org/Obarun/66/-/blob/dev/contributions/docker/dockerfile.dev)
builds **oblibs, 66 and 66-tools from your local working trees** instead, so you
can boot your changes before pushing them.

Because it copies three sibling repositories, its build context is the **parent
directory** that holds them (`oblibs/`, `66/`, `66-tools/`) — not the 66
repository, which cannot reach its siblings:

```
# from the parent directory:
docker build -f 66/contributions/docker/dockerfile.dev -t 66-dev .

# or from inside the 66 repository, pointing the context at the parent:
docker build -f contributions/docker/dockerfile.dev -t 66-dev ..
```

`dockerfile.dev.dockerignore`, next to it, trims that context down to the three
sources.

## Running the container

66 mounts a `tmpfs` on `/run` (the basename of `%%livedir%%`) during the boot.
A container cannot do that without help, so give it one of:

- **`--cap-add SYS_ADMIN`** — 66 performs the mount itself. This is the simple,
  recommended option and is what the examples above use.

Without it, the boot stops early with `unable to mount: /run: Operation not
permitted` and pid 1 exits with `111`.

Once the container is up, pid 1 is [66-scandir](66-scandir.html) and 66's
internal daemons are supervised alongside it:

```
$ docker exec my66 ps -o pid,comm
  PID COMMAND
    1 66-scandir
   10 66-supervise
   ...
   15 66-log
   16 66-shutdownd
   17 66-fdholderd
   18 66-oneshotd
   19 66-eventd
```

You drive the system with the ordinary 66 commands from inside the container:

```
docker exec my66 66 tree status      # the trees and their services
docker exec my66 66 status <service> # one service in detail
docker exec my66 66 log <service>    # its logs
```

### Synchronising the start (optional)

If the container runtime writes to **file descriptor 3**, 66 waits for that write
before continuing its boot. This lets an orchestrator hold the boot until the
environment is ready. If fd 3 is not connected, 66 detects it and proceeds
immediately, so you never have to think about it unless you want the hook.

### CMD, and an entrypoint that prepares the boot

The shipped Dockerfiles end with a `CMD`, not an `ENTRYPOINT`:

```dockerfile
CMD ["/usr/bin/66", "boot", "-c"]
```

That is deliberate: booting is the *default*, but anything you pass after the
image name replaces it. `docker run -it 66-container sh` drops you in a shell with
no 66 running at all — handy to inspect the image or debug the build.

When the container must **prepare** something before booting — render a service
file from environment variables, drop in credentials, restore a snapshot — put it
in an entrypoint script and hand pid 1 over to 66 with `exec`:

```sh
#!/bin/sh
# entrypoint.sh -- runtime setup, then become 66
set -e

cat > /etc/66/service/myapp <<EOF
[Main]
Type = classic

[Start]
Execute = ( /usr/bin/myapp --port ${MYAPP_PORT:-8080} )
EOF
66 enable myapp

exec /usr/bin/66 boot -c "$@"
```

```dockerfile
COPY entrypoint.sh /usr/bin/entrypoint.sh
RUN chmod 755 /usr/bin/entrypoint.sh
ENTRYPOINT ["/usr/bin/entrypoint.sh"]
CMD []
```

**The `exec` is not optional.** Without it the shell stays pid 1, 66 runs as its
child, and you lose everything pid 1 owns: signal handling, reaping orphans, and
the exit code the container reports when you [halt](66-halt.html) it. With `exec`,
66 *is* pid 1 and the rest of this page holds.

The entrypoint runs before the boot and on a writable filesystem, so 66 commands
work there: the `66 enable` above creates the 66 state on the way, exactly as the
first boot would.

## The red thread: supervise a service

A booted-but-empty container is not very interesting. Let us add a real
supervised service and watch 66 keep it alive.

Create an admin service file inside the container, in `%%service_adm%%`:

```
docker exec my66 sh -c 'cat > %%service_adm%%/demo <<EOF
[Main]
Type = classic
Description = "demo heartbeat daemon"

[Start]
Execute = ( loopwhilex foreground { 66-echo "demo heartbeat" } sleep 5 )
EOF'
```

A `classic` service is a long-running process that 66 supervises and restarts if
it dies; the `Execute` field is an [execline](https://skarnet.org/software/execline)
script (replace it with your real daemon, e.g.
`Execute = ( /usr/bin/mydaemon --foreground )`). Enable it **in the default
`global` tree** and start it:

```
docker exec my66 66 enable demo
docker exec my66 66 start demo
docker exec my66 66 status demo
```

Enabling it makes it come up on every boot (the `global` tree is started in the
second wave). If instead you want it up in the first wave, enable it in a
boot-group tree — `66 tree create -o groups=boot core` then
`66 -t core enable demo`; the tree name is yours to choose.

The status reports the service **up**, its pid and uptime, and that a logger was
created for it automatically:

```
Status  : enabled, up (pid 58) (success) 3 seconds, ready 3 seconds by user
...
Dependencies : demo-log
```

Its output is captured and timestamped by the native logger:

```
$ docker exec my66 66 log demo
2026-01-01 12:00:00.000000000  demo heartbeat
2026-01-01 12:00:05.000000000  demo heartbeat
```




### Supervision, not just starting

The point of 66 being pid 1 is that the service is *supervised*, not merely
launched. Kill it and 66 brings it straight back on its own, with a new pid:

```
$ docker exec my66 sh -c 'kill -9 $(66 status demo | grep -oE "pid [0-9]+" | cut -d" " -f2 | head -1)'
$ docker exec my66 66 status demo
Status  : enabled, up (pid 86) (success) 2 seconds, ready 2 seconds by user
```


### Installing a snapshot instead of writing service files

You do not have to author frontend files in the image at all. A
[snapshot](66-snapshot.html) is a copy of an entire 66 system — its trees, its
services and their enabled state — that you can move between machines:

```
66 snapshot create mysnap     # on a machine that already has your services
66 snapshot list
```

It lands in `/var/lib/66/.snapshot/<name>`. Copy that directory into the image
and restore it at build time; the restore also sets the 66 state up, so it works
on a bare image:

```dockerfile
COPY mysnap /var/lib/66/.snapshot/mysnap
RUN 66 snapshot restore mysnap
```

The container then boots with the whole service system already in place —
trees, services and their enabled state exactly as they were on the source
machine.

### User services work too

Nothing here limits 66 to the root system. A regular user inside the container
runs their own 66 exactly as on a normal machine: their own
[scandir](66-scandir.html), their own trees, their own services and their own
loggers, all independent of the root supervision:

```
$ docker exec -u alice my66 sh -c '66 scandir create ; 66 scandir start &
                                   66 enable mysvc ; 66 start mysvc'
$ docker exec -u alice my66 66 status mysvc
In tree : global
Status  : enabled, up (pid 245) (success) 1 seconds, ready 1 seconds by user
Live    : /run/66/scandir/1000/mysvc
```

Their frontend files live in `$HOME/%%service_user%%` and their scandir under
`%%livedir%%/scandir/<uid>`, while the root system keeps `%%livedir%%/scandir/0` —
the two never collide.


Because the service is *enabled*, it comes up on its own every time this image
boots — that is how you turn the walkthrough into a real image: bake your service
files (or a snapshot) and their `enable` into the `Dockerfile`.

## Stopping the container

Because pid 1 is 66, you stop the container by asking 66 to shut down — not by
signalling the container from outside. All three verbs perform a clean shutdown
(trees are brought down, supervisors are stopped) and then make pid 1 exit:

- [66 halt](66-halt.html) — the ordinary "stop this container" verb.
- [66 poweroff](66-poweroff.html) and [66 reboot](66-reboot.html) — same clean
  shutdown, recording a different intent in the halt file for an outer
  supervisor to act on.

```
$ docker exec my66 66 halt
$ docker wait my66
0
```


Stopping the container is not destructive: the system is only down, not undone.
Start the container again and 66 boots it exactly as before — every enabled
service comes back on its own, this time `by boot`:

```
$ docker start my66
$ docker exec my66 66 status demo
Status  : enabled, up (pid 24) (success) 6 seconds, ready 6 seconds by boot
```





By default pid 1 exits `0`. The exit status is the `EXITCODE` value of the halt
file (`%%livedir%%/container/<owner>/halt`), which is `0` on a clean shutdown and
`111` if the boot failed. A `docker run` without `-d` shows the same status as
its own exit code, so a container orchestrator can distinguish a clean stop from
a boot failure.

## Troubleshooting

- **`unable to mount: /run: Operation not permitted`, then exit 111.** The
  container lacks the ability to mount the `tmpfs` on `/run`. Add
  `--cap-add SYS_ADMIN`.

- **`unable to create directory: /var/lib/66/system: Read-only file system`, then
  exit 111.** You mounted `/var/lib` read-only and the image carries no 66 state,
  so the first boot cannot create it. Bake the state at build time (`RUN 66
  status`) or leave `/var/lib` writable — see
  [Read-only /var/lib](#read-only-varlib).

- **`warning: write system version file`.** Harmless: the version marker could not
  be written *yet* during the very early boot. 66 writes it as soon as it sets its
  state up. It only becomes an error if `/var/lib` is read-only (previous entry).

- **`unable to set uid of: 66log` / a chown failure on the first command.** The
  logger user does not exist in the image. Create it (matching your
  `-D 66-log-user`), or build 66 with `-D 66-log-user` pointing at a user that
  does exist.

- **`invalid tree name: <name>`, then the boot dies.** This only happens when you
  pinned a tree with `TREE=<name>` in `init.conf` (or on the kernel command line)
  and that tree does not exist. By default there is no `TREE` key, so the boot
  falls back to the boot group, which is a no-op when empty — never a failure.

- **Where are the boot logs?** The `catch-all` logger writes to
  `%%livedir%%/log/0/current` inside the container; the earliest boot messages
  (before the logger is up) go to the container's own stderr, visible with
  `docker logs`.

## See also

- [66 boot](66-boot.html) — the full boot interface, including `-c` and the
  `init.conf` keys.
- [66 scandir](66-scandir.html) — the supervision directory 66 boots into.
- [66 tree](66-tree.html) — creating and managing trees.
- [Getting started](66-getting-started.html) — writing your first service files.
