# resolve

This command displays the contents of the service's *resolve* file.

This command is purely a debug command used by system administrators or developers.

## Interface

```
resolve [ -h ] [ -n ] [ -f field,... ] service
```

This command displays the contents of the service's *resolve* file. This file are used internally by the `66` program to know *service* information.

[Resolve](66-deeper.html#resolve-files) files are at the core of `66` for service information. They are used internally by `66` to build the dependency graph, ascertain file locations, storing the parse process result of a service, and other critical aspects of a service.

## Options

- **-h, --help**: prints this help.

- **-n, --no-name**: do not display the field name, only its value. Useful for scripting.

- **-f, --field** *field,...*: comma separated list of fields to display.

## Usage example

Displays the *resolve* file of service `foo`. The file holds the full result of
the parse — roughly ninety fields. The excerpt below shows the most telling ones
(identity, type, the generated run script, on-disk locations and logger
settings):

```
$ 66 resolve foo
name            : foo
description     : my first service
version         : 0.8.2.1
type            : 1
treename        : global
enabled         : 0
owner           : 0
user            : root
depends         : None
requiredby      : None
frontend        : /etc/66/service/foo
src_servicedir  : /var/lib/66/system/service/svc/foo
run             : #!/usr/bin/execlineb -P
importas -D2 VERBOSITY VERBOSITY
/usr/libexec/66-execute -v${VERBOSITY} start foo
[...]
logname         : foo-log
logbackup       : 3
logmaxsize      : 1000000
stdouttype      : 3
stdoutdest      : /var/log/66/foo
rversion        : 0.8.2.1
```

`type` is the numeric service type (`0` classic, `1` oneshot, `2` module — here
`1` because `foo` is a oneshot). The `run` field is the actual script `66`
generated from your
`[Start]` section and that supervision executes. Fields ending in `dir`/`dest`
are the resolved on-disk and live paths — see
[deeper understanding](66-deeper.html#resolve-files).
