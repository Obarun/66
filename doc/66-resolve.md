# resolve

This command displays the contents of the service's *resolve* file.

This command is purely a debug command used by system administrators or developers.

## Interface

```
resolve [ -h ] [ -n ] [ -f field,... ] service
```

This command displays the contents of the service's *resolve* file. This file is used internally by the `66` program to know *service* information.

[Resolve](66-deeper.html#resolve-files) files are at the core of `66` for service information. They are used internally by `66` to build the dependency graph, ascertain file locations, storing the parse process result of a service, and other critical aspects of a service.

## Options

- **-h, --help**: prints this help.

- **-n, --no-name**: do not display the field name, only its value. Useful for scripting.

- **-f, --field** *field,...*: comma separated list of fields to display.

## Usage example

Displays the *resolve* file of the oneshot `foo`, parsed from this frontend:

```
[Main]
Type = oneshot
Description = "my first service"
Version = 0.0.1
User = ( root )

[Start]
Execute = ( echo foo )
```

The file holds the full result of the parse, one line per field described in
the next section. The excerpt below shows the most telling ones, identity, the
generated scripts, on-disk locations and the output redirection:

```
$ 66 resolve foo
name             : foo
description      : my first service
version          : 0.0.1
type             : 1
[...]
treename         : global
user             : root
[...]
enabled          : 0
islog            : 0
logger           : 0
[...]
frontend         : /etc/66/service/foo
src_servicedir   : /var/lib/66/system/service/svc/foo
depends          : None
requiredby       : None
[...]
run              : #!/usr/bin/execlineb -P
/usr/libexec/66-execute start foo

run_user         : #!/usr/bin/execlineb -P
 echo foo 

run_build        : None
[...]
timeoutstart     : 0
timeoutstop      : 10000
[...]
maxdeath         : 5
maxdeathtime     : 30000
[...]
stdouttype       : 3
stdoutdest       : /var/log/66/foo
[...]
rversion         : 0.9.2.0
```

`type` is `1` because `foo` is a oneshot, and a oneshot never has a logger,
hence `logger` at `0`. `run` is the wrapper script 66 writes, which calls
`66-execute`. `run_user` is the script built from `[Start] Execute`: the key
does not start with a shebang, so 66 wraps it in execlineb and `run_build` stays
`None`. Fields ending in `dir` or `dest` are the resolved on-disk and live
paths, see [deeper understanding](66-deeper.html#resolve-files).

## Fields and their origin

Each field below is listed with where its value comes from. **Frontend** names
the [frontend](66-frontend.html) key that sets it, as `[Section] Key`. **internal**
means 66 computes or records the value itself: no key of the frontend sets it.

### Identity and supervision

| Field | Frontend | Notes |
|---|---|---|
| `name` | file name of the frontend | the instance name for an [instantiated](66-instantiated-service.html) service |
| `description` | `[Main] Description` | |
| `version` | `[Main] Version` | |
| `type` | `[Main] Type` | `0` classic, `1` oneshot, `2` module, `3` event |
| `earlier` | `[Main] Flags` | `1` when `Flags` holds `earlier` |
| `copyfrom` | `[Main] CopyFrom` | |
| `intree` | `[Main] InTree` | |
| `ownerstr`, `owner` | internal | name and uid of the account the service is parsed for |
| `treename` | internal | tree the service belongs to: `[Main] InTree`, the `-t` option or the default tree |
| `user` | `[Main] User` | |
| `inns` | internal | module the service is a member of |
| `enabled` | internal | set by [66 enable](66-enable.html) and [66 disable](66-disable.html) |
| `islog` | internal | `1` for a logger 66 created |
| `logger` | `[Main] Options` | `1` when the service has a logger, that is without `!log`; always `0` for a oneshot and a module |
| `has_limit`, `has_environ`, `has_io`, `has_execute`, `has_dependencies`, `has_regex`, `has_event` | internal | `1` when the matching part of the resolve file was written |
| `home` | internal | base directory of the account |
| `frontend` | internal | path of the frontend file that was parsed |
| `src_servicedir` | internal | service directory under `home` |
| `rversion` | internal | version of 66 that wrote the resolve file |

### Dependencies

| Field | Frontend | Notes |
|---|---|---|
| `depends` | `[Main] Depends` | |
| `requiredby` | `[Main] RequiredBy` | |
| `optsdeps` | `[Main] OptsDepends` | |
| `contents` | internal | members of a module, read from the module directory |
| `provide` | `[Main] Provide` | |
| `conflict` | `[Main] Conflict` | |
| `ndepends`, `nrequiredby`, `noptsdeps`, `ncontents`, `nprovide`, `nconflict` | internal | number of names in the field above |

### Start, stop and process

| Field | Frontend | Notes |
|---|---|---|
| `run` | internal | wrapper script 66 writes, which calls `66-execute` |
| `run_user` | `[Start] Execute` | |
| `run_build` | internal | `custom` when `[Start] Execute` starts with a shebang, `None` otherwise: 66 wraps the script in execlineb |
| `run_runas` | `[Start] RunAs` | |
| `finish` | internal | wrapper script 66 writes for the stop |
| `finish_user` | `[Stop] Execute` | |
| `finish_build` | internal | `custom` when `[Stop] Execute` starts with a shebang, `None` otherwise: 66 wraps the script in execlineb |
| `finish_runas` | `[Stop] RunAs` | |
| `timeoutstart` | `[Start] Timeout` | `0` when the key is not written: no start timeout |
| `timeoutstop` | `[Stop] Timeout` | the general stop delay chosen at build time when the key is not written |
| `notify` | `[Start] Notify` | `0` when the key is not written: no readiness notification |
| `maxdeath` | `[Start] MaxDeath` | `5` when the key is not written; `0` disables the crash budget |
| `maxdeathtime` | `[Start] MaxDeathInterval` | in milliseconds, `30000` when the key is not written |
| `down` | `[Main] Flags` | `1` when `Flags` holds `down` |
| `downsignal` | `[Stop] DownSignal` | `0` when the key is not written, which means `SIGTERM` |
| `blockprivileges` | `[Execute] BlockPrivileges` | |
| `umask` | `[Execute] UMask` | |
| `want_umask` | internal | `1` when `[Execute] UMask` is written |
| `nice` | `[Execute] Nice` | |
| `want_nice` | internal | `1` when `[Execute] Nice` is written |
| `chdir` | `[Execute] ChangeDirectory` | |
| `capsbound` | `[Execute] CapsBound` | |
| `capsambient` | `[Execute] CapsAmbient` | |
| `ncapsbound`, `ncapsambient` | internal | number of capabilities in the field above |

### Input and output

| Field | Frontend | Notes |
|---|---|---|
| `stdintype`, `stdindest` | `[Execute] StdIn` | the kind of redirection and its destination |
| `stdouttype`, `stdoutdest` | `[Execute] StdOut` | |
| `stderrtype`, `stderrdest` | `[Execute] StdErr` | |

### Resource limits

| Field | Frontend | Notes |
|---|---|---|
| `limitas`, `limitcore`, `limitcpu`, `limitdata`, `limitfsize`, `limitlocks`, `limitmemlock`, `limitmsgqueue`, `limitnice`, `limitnofile`, `limitnproc`, `limitrtprio`, `limitrttime`, `limitsigpending`, `limitstack` | `[Execute] LimitAS` ... `[Execute] LimitSTACK` | one field per `LimitXXX` key, same name in lower case |

### Environment

| Field | Frontend | Notes |
|---|---|---|
| `env` | `[Environment]` | the `key=value` pairs of the section |
| `envdir` | internal | directory where 66 writes the configuration file of the service |
| `env_overwrite` | internal | set by the `-I` option of [66 parse](66-parse.html) |
| `importfile` | `[Environment] ImportFile` | |
| `nimportfile` | internal | number of files in `importfile` |

### Module substitutions

| Field | Frontend | Notes |
|---|---|---|
| `configure` | `[Regex] Configure` | |
| `directories` | `[Regex] Directories` | |
| `files` | `[Regex] Files` | |
| `infiles` | `[Regex] InFiles` | |
| `ndirectories`, `nfiles`, `ninfiles` | internal | number of entries in the field above |

### Events

| Field | Frontend | Notes |
|---|---|---|
| `eventtype` | `[Event] EventType` | for an event source (`Type = event`), `[Main] EventType` |
| `eventfrom` | `[Event] From` | |
| `neventfrom` | internal | number of names in `eventfrom` |
| `eventon` | `[Event] On` or `[Event] OnAll` | for an `inotify` source, `[Main] On` |
| `neventon` | internal | number of conditions in `eventon` |
| `eventcombine` | `[Event] On` or `[Event] OnAll` | `0` for `On`, `1` for `OnAll` |
| `eventdo` | `[Event] Do` | |
| `eventemit` | `[Event] Emit` | |
| `eventpropagate` | `[Event] Propagate` | |
| `eventwatch` | `[Main] Watch` | `inotify` source |
| `eventexpression` | `[Main] Expression` | `schedule` source |
| `eventtimezone` | `[Main] Timezone` | `schedule` source |
| `eventinterval` | `[Main] Every` | `timer` source |

### Live directories

| Field | Frontend | Notes |
|---|---|---|
| `livedir`, `status`, `live_servicedir`, `scandir`, `statedir`, `eventdir`, `supervisedir`, `fdholderdir`, `oneshotddir`, `eventddir` | internal | paths under the live directory, `/run/66` by default, computed by 66 for the service |
