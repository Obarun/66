# Deeper understanding

This documentation explains the internal structure of `66` on the system and the roles of the different directories and file components.

**Never manually change** any directories or files within the `66` ecosystem, as this is the best way to break it.

## The service pipeline

Everything below is a stage in one pipeline, from the file you write to a
running, supervised process:

```
 frontend file ─▶ parse ─▶ resolve (CDB) ─▶ tree ─▶ scandir ─▶ 66-supervise ─▶ running service
   you write      compile   parsed state    group    live dir   one per svc
```

- **frontend file** — your INI declaration. See [frontend](66-frontend.html).
- **parse** — compiles it, applying [identifiers](66-identifier.html) and validation. See [parse](66-parse.html).
- **resolve** — the compiled result, stored as a CDB-backed [resolve file](#resolve-files). See [resolve](66-resolve.html).
- **tree** — the named group the service belongs to. See [tree](66-tree.html).
- **scandir** — the live supervision directory that `66-scandir` watches. See [scandir](66-scandir.html).
- **[66-supervise](66-supervise.html)** — one per service, keeps it in the state you asked for.

The rest of this page walks the on-disk directories that hold these stages.

## %%system_dir%%

This directory is specified at compile time by using the `-D system-dir=` option to `meson setup`.

This directory stores trees and services configuration files. You should see this following structure

```
%%system_dir%%
└── system
    ├── .resolve
    │   ├── service
    │   │    ├── <service> -> %%system_dir%%/system/service/svc/<service>
    │   │    └── <service> -> %%system_dir%%/system/service/svc/<service>
    │   ├── Master
    │   ├── <tree> resolve file
    │   └── <tree> resolve file
    └── service
        └── svc
            ├── <service>
            │   ├── .resolve
            │   │   └── <service> resolve file
            │   ├── state
            │   │   └── status file
            │   ├── file
            │   └── file
            ├── <service>
            │   ├── .resolve
            │   │   └── <service> resolve file
            │   ├── state
            │   │   └── status file
            │   ├── file
            │   └── file
            └── <service>
                ├── .resolve
                │   └── <service> resolve file
                ├── state
                │   └── status file
                ├── file
                └── file
```

The exact same structure is also available for regular users at the `${HOME}/%%user_dir%%` directory, specified at compile time by using the `-D user-dir=` option to `meson setup`.

Every system or administrator directory documented below has a user counterpart, and each one has its own compile-time option. They mirror each other one for one:

| System / administrator | Regular user | Option | Default |
| --- | --- | --- | --- |
| `%%system_dir%%` | `${HOME}/%%user_dir%%` | `-D user-dir=` | `.66` |
| `%%system_log%%` | `${HOME}/%%user_log%%` | `-D user-log-dir=` | `.66/log` |
| `%%service_adm%%` | `${HOME}/%%service_user%%` | `-D user-service-dir=` | `.66/service` |
| `%%service_admconf%%` | `${HOME}/%%service_userconf%%` | `-D user-service-conf-dir=` | `.66/conf` |
| `%%script_system%%` | `${HOME}/%%script_user%%` | `-D user-script-dir=` | `.66/script` |
| `%%seed_adm%%` | `${HOME}/%%seed_user%%` | `-D user-seed-dir=` | `.66/seed` |
| `%%environment_adm%%` | `${HOME}/%%environment_user%%` | `-D user-environment-dir=` | `.66/environment` |

**Never pass an absolute path to a `user-*` option.** The value is relative and `${HOME}` is prepended at runtime for whoever owns the process, which is what lets a single build serve every user on the machine. Setting `user-dir=/home/alice/.66` would point every user at Alice's directory.

The [live directory](#livedir) is not duplicated per user: it is shared, and the per-user parts are keyed by `UID` inside it, as `%%livedir%%/state/UID` and `%%livedir%%/environment/UID`.

The `%%system_dir%%/system/.resolve` directory contains resolve files for trees. Each tree have its own resolve file, while the [Master](#master-resolve-file) resolve file contains information about all trees available on the system.

The `%%system_dir%%/system/.resolve/service` directory consists of symlinks that point to resolve files for each service. These symlinks enable `66` to quickly access the resolve files for individual services, facilitating the construction of the complete graph of services interdependences.

The `%%system_dir%%/system/service/svc` directory includes service directories, each housing the results of the [parse](66-parse.html) process, as well as internal directories and files essential for `66`.

### %%system_dir%%/system/.resolve/<tree>

The content of the resolve file for a tree, such as `%%default_treename%%`, can be viewed using the [66 tree resolve](66-tree.html#resolve) command in the following manner

```
66 tree resolve global
```

This displays information in a self-explanatory manner

```
name        : global
depends     : None
requiredby  : session
allow       : 0
groups      : admin
contents    : consolekit consolekit-log dbus dbus-log networkmanager networkmanager-log
ndepends    : 0
nrequiredby : 1
nallow      : 1
ngroups     : 1
ncontents   : 6
```

This information is utilized by `66` to fulfill the user-requested commands. It contains precisely what is necessary and nothing more.

For instance, in a command like `66 tree admin -o depends=security global`, the `security` tree name is appended to the `depends` field, incrementing the `ndepends` count by one.

Essentially, trees are just resolve files - keeping it simple, straightforward, and easy.

#### Master resolve file

The `Master` resolve file holds significant importance as it contains information about all trees. You can access its contents using the `66 tree resolve Master` command, which presents information in a self-explanatory format

```
name        : Master
allow       : root
enabled     : global session
current     : global
contents    : global session boot
nallow      : 1
nenabled    : 2
ncontents   : 3
```

The `Master` resolve file isn't accessible for direct user use. Attempting to execute commands like `66 tree enable Master` will result in an error.

This file serves `66` in rapidly accessing an overview of trees on the system. When a user triggers a `66 tree disable session` command, it updates the `enabled` and `nenabled` fields to reflect the current state of the tree.

### %%system_dir%%/system/service/svc/\<service\>

Every service possesses its individual directory. At its core, this directory houses the outcome of the [parse](66-parse.html) process, heavily reliant on the corresponding [frontend](66-frontend.html) file. Yet, all service directories invariably include the `.resolve` and `state` subdirectories.

#### %%system_dir%%/system/service/svc/\<service\>/.resolve

This directory stores the resolve file for each service, mirroring how `%%system_dir%%/system/.resolve/` houses the resolve file for a tree. Running `66 resolve \<service\>` showcases the content of this file, presenting information similar to the following

```
name             : dhcpcd
description      : dhcpcd daemon
version          : 0.8.0
type             : 0
earlier          : 0
copyfrom         : None
intree           : None
ownerstr         : 0
owner            : 0
treename         : global
user             : root
inns             : None
enabled          : 1
islog            : 0
logger           : 1
has_limit        : 0
has_environ      : 1
has_io           : 1
has_execute      : 1
has_dependencies : 1
has_regex        : 0
has_event        : 0
home             : /var/lib/66/
frontend         : /usr/share/66/service/dhcpcd
src_servicedir   : /var/lib/66/system/service/svc/dhcpcd
depends          : dhcpcd-log
requiredby       : None
optsdeps         : None
contents         : None
provide          : None
conflict         : None
ndepends         : 1
nrequiredby      : 0
noptsdeps        : 0
ncontents        : 0
nprovide         : 0
nconflict        : 0
run              : #!/usr/bin/execlineb -P
importas -D2 VERBOSITY VERBOSITY
/usr/libexec/66-execute -v${VERBOSITY} start dhcpcd

run_user         : #!/usr/bin/execlineb -P
 /usr/bin/execl-cmdline -s { /usr/bin/dhcpcd ${ArgsStart} }

run_build        : None
run_runas        : None
finish           : #!/usr/bin/execlineb -S0
importas -D2 VERBOSITY VERBOSITY
/usr/libexec/66-execute -v${VERBOSITY} stop dhcpcd $@

finish_user      : #!/usr/bin/execlineb -P
 /usr/bin/execl-cmdline -s { /usr/bin/dhcpcd ${ArgsStop} }

finish_build     : None
finish_runas     : None
timeoutstart     : 0
timeoutstop      : 0
down             : 0
downsignal       : 0
blockprivileges  : 0
umask            : 0
want_umask       : 0
nice             : 0
want_nice        : 0
chdir            : None
capsbound        : None
capsambient      : None
ncapsbound       : 0
ncapsambient     : 0
notify           : 0
maxdeath         : 5
maxdeathtime     : 30000
livedir          : /run/66/
status           : /var/lib/66/system/.resolve/service/dhcpcd/state/status
live_servicedir  : /run/66/state/0/dhcpcd
scandir          : /run/66/scandir/0/dhcpcd
statedir         : /run/66/state/0/dhcpcd/state
eventdir         : /run/66/state/0/dhcpcd/event
supervisedir     : /run/66/state/0/dhcpcd/supervise
fdholderdir      : /run/66/scandir/0/fdholder
oneshotddir      : /run/66/scandir/0/oneshotd
eventddir        : /run/66/scandir/0/eventd
env              : ArgsStart=!-B ${ArgsConfFile}
ArgsStop=!-x
ArgsConfFile=!-f /etc/dhcpcd.conf

envdir           : /etc/66/conf/dhcpcd
env_overwrite    : 0
importfile       : None
nimportfile      : 0
configure        : None
directories      : None
files            : None
infiles          : None
ndirectories     : 0
nfiles           : 0
ninfiles         : 0
stdintype        : 3
stdindest        : /run/66/scandir/0/fdholder
stdouttype       : 3
stdoutdest       : /var/log/66/dhcpcd
stderrtype       : 5
stderrdest       : /var/log/66/dhcpcd
limitas          : 0
limitcore        : 0
limitcpu         : 0
limitdata        : 0
limitfsize       : 0
limitlocks       : 0
limitmemlock     : 0
limitmsgqueue    : 0
limitnice        : 0
limitnofile      : 0
limitnproc       : 0
limitrtprio      : 0
limitrttime      : 0
limitsigpending  : 0
limitstack       : 0
eventtype        : 0
eventfrom        : None
neventfrom       : 0
eventon          : None
neventon         : 0
eventcombine     : 0
eventdo          : 0
eventemit        : None
eventwatch       : None
eventexpression  : None
eventtimezone    : None
eventinterval    : 0
rversion         : 0.9.0.0
```

The resolve file is the full, low-level picture. For day-to-day use, the [66 status](66-status.html) command presents a readable summary drawn from this file and from the service's runtime record, rather than dumping every field.

A service is not stored as one monolithic record. A small *core* record — the fields down to `src_servicedir`, plus the `live*` paths and `rversion` — is always written, and the rest lives in *addons* written only when the frontend needs them: dependencies, execute, environ, regex, io, limit and event. The `has_*` fields say which addon exists on disk. `66 resolve` always prints every field, so a field belonging to an absent addon shows `None` for a string and `0` for a number: `dhcpcd` above declares no `[Event]` section, hence `has_event : 0` and a whole `event*` block reading `None`/`0`.

For a service that *does* carry an [[Event]](66-frontend.html#section-event) section — a [reactor](66-event.html#declaring-a-reactor-the-event-section) — the same block is filled in. A `backend` service declaring

```ini
[Event]
EventType = service
From = ( rabbitmq )
On = ( down )
Do = stop
Emit = backend-down
```

resolves to

```
has_event        : 1
...
eventtype        : 0
eventfrom        : rabbitmq
neventfrom       : 1
eventon          : down
neventon         : 1
eventcombine     : 0
eventdo          : 2
eventemit        : backend-down
eventwatch       : None
eventexpression  : None
eventtimezone    : None
eventinterval    : 0
```

The numeric fields are enumerations, not flags:

- `eventtype` is the `EventType` key: `0` service, `1` signal, `2` inotify, `3` schedule, `4` user, `5` timer.
- `eventcombine` records which condition key was used: `0` for `On` (any condition fires), `1` for `OnAll` (every condition must hold at once).
- `eventdo` is the `Do` key: `0` none, `1` start, `2` stop, `3` restart, `4` reload, `5` reconfigure, `6` free.
- `eventfrom` and `eventon` hold the lists verbatim, space separated, with `neventfrom` and `neventon` counting them.
- `eventwatch`, `eventexpression`, `eventtimezone` and `eventinterval` belong to the source families — `Watch` for inotify, `Expression`/`Timezone` for schedule, `Every` for timer — and stay empty on a reactor.

See [the event system](66-event.html) for the grammar these fields come from.

Some precision is needed here:

- The intree field corresponds to the `InTree` field in the [frontend](66-frontend.html) file, while `treename` indicates the current associated tree of the service. Changing the tree with `66 -t <tree> enable <service>` modifies `treename`, leaving `intree` unchanged.

- The `inns` field indicates whether the service is a part of a `module`.

- The `run` and `finish` field contains the content of the `%%system_dir%%/system/service/svc/<service>/run` and `%%system_dir%%/system/service/svc/<service>/finish` file respectively.

- Meanwhile, `run_user`, and `finish_user` fields are derived from the [[Start]](66-frontend.html#section-start) and [[Stop]](66-frontend.html#section-stop) sections in the frontend file. Specifically, `run_user` corresponds to `Execute` in the [[Start]](66-frontend.html#section-start) section, and the others function similarly but for the [[Stop]](66-frontend.html#section-stop) section.

- Other fields like `ownerstr`, `home`, `frontend`, `src_servicedir`, `livedir`, `status`, `live_servicedir`, `scandir`, `statedir`, `eventdir`, `supervisedir`, `fdholderdir`, `oneshotddir`, `eventddir`, `logger` and `env_overwrite` are used internally for `66`'s operations.

#### %%system_dir%%/system/service/svc/\<service\>/state

This directory houses a *binary* file named `status`, which `66` uses to track the service's *management* state — what has been parsed, supervised, or is pending. Running `66 state <service>` displays output similar to the following

```
toinit          : 0
toreload        : 0
torestart       : 0
tounsupervise   : 0
toparse         : 0
isparsed        : 1
issupervised    : 1
```

For instance, when executing `66 free <service>`, the `tounsupervise` field switches to `1` at the process start and back to `0` at the end. The `issupervised` field also becomes `0`.

This *management* state is distinct from the *runtime* state — whether the process is actually running right now, since when, and with which result. The runtime state is a separate binary record written by [66-supervise](66-supervise.html) under the live directory, at `%%livedir%%/state/UID/<service>/supervise/status`. A service is therefore described by three complementary views:

- [66 state](66-state.html) — the *management* flags shown above (parsed, supervised, pending actions).
- [66 runstate](66-runstate.html) — the raw *runtime* record (`state`, `result`, `who`, `pid`, `code`, timestamps, `ndeaths`).
- [66 status](66-status.html) — a human-readable summary combining both.

## %%skel%%

This directory is specified at compile time by using the `-D skeleton-dir=` option to `meson setup`.

It holds the boot and shutdown skeleton files together with the system administrator's own configuration subdirectories

```
%%skel%%
├── init.conf              boot configuration file, see 66-boot
├── conf                   %%service_admconf%%
├── environment            %%environment_adm%%
├── seed                   %%seed_adm%%
└── service                %%service_adm%%
```

The `init.conf` and `rc.*` files are the skeleton files read at boot and shutdown; they are described in [boot](66-boot.html). The four subdirectories below are reserved for the system administrator.

Users may manage these subdirectories, with the exception of `%%service_admconf%%`, which is handled by the [configure](66-configure.html) command.

### %%service_admconf%%

This directory is specified at compile time by using the `-D sysadmin-service-conf-dir=` option to `meson setup`.

This directory stores the configuration file of a service, which is the outcome of the [[Environment]](66-frontend.html#section-environment) section parsed from the [frontend](66-frontend.html) file.

The `66 configure <service>` command handles this directory. It's advised for users, including system administrators, to avoid direct interaction with these directories and utilize the [configure](66-configure.html) command instead.

Its subdirectories contain versioned files per service and are self-explanatory.

### %%environment_adm%%

This directory is specified at compile time by using the `-D sysadmin-environment-dir=DIR` option to `meson setup`.

This directory serves as a location for system administrators to provide default environment variables used by a [scandir](66-scandir.html) at runtime and propagated to services handled by the scandir.

See the [Environment](66-scandir.html#environment) explanation for further information about the behavior, the syntax and the limitations.

### %%seed_adm%%

This directory is specified at compile time by using the `-D sysadmin-seed-dir=` option to `meson setup`.

This directory is managed by the user and stores [seed](66-tree.html#seed-files) files, used, for example, by the [tree](66-tree.html) command. If a [seed](66-tree.html#seed-files) file exists both in this directory and at `%%service_system%%`, the one in this directory takes precedence.

It's crucial for system administrators to avoid altering this directory or its subdirectories, ensuring users can manage it without losing their changes.

### %%service_adm%%

This directory is specified at compile time by using the `-D sysadmin-service-dir=` option to `meson setup`.

This directory is designated for users to store [frontend](66-frontend.html) files. If a [frontend](66-frontend.html) file for a service exists both in this directory and at `%%service_system%%`, the file in this directory takes precedence.

It's crucial for system administrators to avoid altering this directory or its subdirectories, ensuring users can manage it without losing their changes.

## %%service_system%%

This directory is specified at compile time by using the `-D system-service-dir=` option to `meson setup`.

This directory is reserved for system administrators to install [frontend](66-frontend.html) file, often through package manager. Any modifications made by users may be overwritten during system updates.

## %%script_system%%

This directory is specified at compile time by using the `-D system-script-dir=` option to `meson setup`.

This directory serves as a location for system administrators to install additional scripts required by [frontend](66-frontend.html) files or those related to the `module` service type. It's a secure area for administrators to manage scripts effectively.

## %%seed_system%%

This directory is specified at compile time by using the `-D system-seed-dir=` option to `meson setup`.

this directory is intended for system administrators to install [seed](66-tree.html#seed-files) files. User modifications in this directory may be lost during system updates.

## %%livedir%%

This directory is specified at compile time by using the `-D livedir=` option to `meson setup`. It also can be specified on-the-fly with the [66 -l](66.html) option.

This should be within a writable and executable filesystem, likely a RAM filesystem, mounted with `exec` and `rw` mount flag.

This directory and its subdirectories are managed by `66`. Users, including system administrators, should avoid directly interacting with these directories.

It is created at [66 scandir create](66-scandir.html#start) invocation if it does not exist yet. You should see the following structure

```
%%livedir%%
├── scandir
│   └── 0                                  one directory per UID (0 is root)
│       ├── .66-scandir                    control of the native 66-scandir
│       │   ├── control                    command FIFO
│       │   ├── lock
│       │   └── SIGINT, SIGTERM, finish…   signal and lifecycle scripts
│       ├── scandir-log                    scandir internal logger (66-log)
│       ├── fdholderd                      fdholder daemon (fdholderdir field)
│       ├── oneshotd                       oneshot daemon (oneshotddir field)
│       ├── eventd                         event daemon (eventddir field)
│       ├── <service> -> ../../state/0/<service>     classic: supervised symlink (scandir field)
│       ├── .<service> -> ../../state/0/<service>    oneshot/module: hidden symlink, skipped by 66-scandir
│       └── container                      container mode only (66 scandir create -B)
├── state
│   └── 0                                  one directory per UID
│       └── <service>                      runtime copy of the service (live_servicedir field)
│           ├── run, finish, …             scripts copied from %%system_dir%%
│           ├── .resolve -> %%system_dir%%/system/service/svc/<service>/.resolve
│           ├── state
│           │   └── status                 management flags, see 66 state (statedir field)
│           ├── supervise
│           │   └── status                 runtime record, see 66 runstate (supervisedir field)
│           └── event                      supervision event fifodir (eventdir field)
├── environment
│   └── 0                                  one directory per UID
│       └── <variable>                     one file per published variable, see 66 env
└── log
    └── 0                                  destination of the scandir-log output
        └── current                        uncaught logs
```

The annotations in parentheses are the resolve fields ([66 resolve](66-resolve.html)) that hold each path.

### %%livedir%%/log

This directory houses log files, usually one per service. It's commonly used for services initiated before a rewritable `/var/log` directory becomes available.

### %%livedir%%/log/0

This directory stores logs generated by `scandir-log` service, which is internally created by `66`. During the boot, the [stage2](66-boot.html) process brings up the services and captures output using `scandir-log`.  It might also contain `uncaught-log` entries from services lacking their own logger, making it valuable for debugging purposes.

### %%livedir%%/scandir

This directory is managed through the [scandir](66-scandir.html) comand. Users, including system administrators, should avoid directly interacting with these directories.

#### %%livedir%%/scandir/UID

Given `66` can run with root or regular account privileges, this directory contains subdirectories. Each account possesses its own scandir specified by its number. For example, `%%livedir%%/scandir/0` is owned by root, while `%%livedir%%/scandir/1000` typically belongs to the first regular account created on the system.

This directory consists of service symlinks that point to their corresponding `%%livedir%%/state/UID/<service>` directory. A *classic* service is symlinked under its plain name, so `66-scandir` supervises it. A *oneshot* or a *module*, which is not supervised, is symlinked under a name prefixed with a dot (`.<service>`), which `66-scandir` skips.

It also holds the `.66-scandir` control directory of the running [66-scandir](66-scandir.html) process, along with the `scandir-log` logger and the `fdholderd`, `oneshotd` and `eventd` daemons.

*note*: The `66 scandir create -B` invocation create the directory `%%livedir%%/scandir/UID/container` containing a named file *halt*. See [boot](66-boot.html) for further information.

### %%livedir%%/state

This directory is managed internally by `66` and contains directories and files needed for the run time of services. Users, including system administrators, should avoid directly interacting with these directories.

### %%livedir%%/state/UID

As `66` can be executed with root or regular account privileges, this directory contains subdirectories. Each account has its own `state` specified by its number. For instance, `%%livedir%%/state/0` is owned by root, whereas `%%livedir%%/state/1000` is typically owned by the first regular account created on the system.

For instance, the `%%livedir%%/state/0/<service>` contains a verbatim copy of the `%%system_dir%%/system/service/svc/<service>` for each root service.

Invocating the `66 free <service>` remove the corresponding `%%livedir%%/state/0/<service>` directory.

At [start](66-start.html) command executed will created the corresponding `%%livedir%%/state/0/<service>` if it doesn't exist yet.

### %%livedir%%/environment

This directory holds the *runtime environment*: the per-session variables published with the [66 env](66-env.html) command. It is created by [66 scandir create](66-scandir.html#create) with the sticky bit set (mode `1777`), like `%%livedir%%/scandir`, `%%livedir%%/state` and `%%livedir%%/log`, so that every account can own its own subdirectory below it.

It is also where the temporary file of a publication is created, for the time of a `rename` into the subdirectory below. Nothing else lives here.

### %%livedir%%/environment/UID

One directory per account, mode `0755` and owned by that account. It holds **one file per published variable**, named after the variable and containing the single `variable=value` pair — `%%livedir%%/environment/1000/DISPLAY` contains `DISPLAY=:0`.

`66-execute` merges this directory into the environment of **every** service of the scandir at each start, last and verbatim, so a published value overrides the frontend, the service configuration and the `ImportFile` files, and reaches services that declare no `[Environment]` section at all. See [66 env](66-env.html) for the precedence chain and the limits.

The directory lives and dies with the scandir it belongs to: [66 scandir remove](66-scandir.html#remove) destroys it, so nothing published survives it. `66 env` never creates it — it reports its absence rather than publishing into a store no service reads.

Because the whole directory is read by `environ_merge_dir`, it is subject to the limits of an [environment directory](66-scandir.html#environment): at most `20` files, hence at most 20 published variables, and no empty file. This is why a publication never creates its temporary here.

## %%system_log%%

This directory is specified at compile time by using the `-D system-log-dir=` option to `meson setup`.

This directory is automatically managed by `66`. Users, including system administrators, should avoid directly interacting with these directories.

Each service, if a logger is associated, has its own subdirectory at `%%system_log%%/<service>`.

User can control the rotation of the log file with:

- the `Backup` field at the [[Logger]](66-frontend.html#section-logger) section of the [frontend](66-frontend.html) file.

- invocating `66 reload -P <service>` or `66 signal -aP <service>` command.

## Resolve files

*Resolve* files are essentially [CDB](http://cr.yp.to/cdb.html) databases. They are independent of extra libraries, running daemons, or third-party programs. These files are lightweight and efficient to read and write. However, the downside is the inability to upgrade a field without rewriting the entire database whenever modifications are made to any fields.

The size of the database varies based on its contents and typically ranges between 4-7 kilobytes.
