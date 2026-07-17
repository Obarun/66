# status

This command displays the state of a service.

`66 status` answers one question: what is this service doing right now. It is not
a dump of everything known about it. The parsed configuration of a service — its
run script, environment, standard io redirections, frontend location — belongs to
[66 resolve](66-resolve.html), and its full log to [66 log](66-log.html).
`status` shows the state, the dependency links, and the last few log lines for a
quick glance.

## Interface

```
status [ -h ] [ -n ] [ -f field,... ] [ -g ] [ -d depth ] [ -r ] service
```

By default the dependency graph is rendered in the [start](66-start.html) order of execution. You can reverse the rendered order, meaning the [stop](66-stop.html) execution, with the `-r` option.

Without specifying `-f`, all fields are displayed. The `contents` field only
means something for a [module](66-module-usage.html), so it is left out of the
default display for any other service type; asking for it explicitly with `-f`
stays valid.

If no *service* is specified, it displays all services from all trees. This is a useful way to quickly get an overview of the entire service system. In that case, `-g` is implied and `-d` can be used but `-f` and `-n` options have no effect.

## Options

- **-h, --help**: prints this help.

- **-n, --no-name**: do not display the field name(s) specified. Combining this option with `-f` facilitates scripting usage. Durations are printed as raw seconds in that mode, which is the form a script wants.

- **-f, --field** *field,...*: comma separated list of fields to display.

- **-g, --graph**: shows the dependency list of the *service* as a hierarchical graph instead of a list.

- **-d, --depth** *depth*: limits the depth of the dependency list visualisation; default is 1. This implies **-g** option.

- **-r, --reverse**: shows the dependency list of *services* in reverse mode.

- **-o, --options** *field,...*: deprecated alias for `-f`; use `-f` instead.

## Valid fields for -f option

- **name**: displays the name.
- **status**: displays the run state. See *Reading the status field* below.
- **description**: displays the description.
- **type**: displays the service type.
- **source**: displays the path of the service's [frontend](66-frontend.html) file.
- **tree**: displays the service's tree name.
- **enabled**: `yes` if the service comes up at boot, `no` if it does not.
- **pid**: the pid of the running process, `0` when there is none. Always a number, so a script never has to special-case a word.
- **depends**: displays the service's dependencies.
- **requiredby**: displays the service(s) which depends on service.
- **contents**: displays services within module.
- **log**: displays the last five lines of the service's log. Use [66 log](66-log.html) to read the whole log, filter it or follow it.

## Reading the status field

The `status` field combines two **independent** states, the **enable state** and
the **run state**, and tells how long the service has been in the latter:

```
Status : enabled, up (pid 620) since 8h 59min by boot
```

The **enable state** is `enabled` (the service comes up at boot) or `disabled`
(it does not). It states an intent for the next boot and says nothing about the
running process: a service can be `disabled, up` (started by hand, it will not
return after a reboot) or `enabled, down` (it will come up next boot).

The **run state** is one of `up`, `down`, `starting`, `stopping`, `finishing`,
`restarting`, `done`, `failed` or `waiting`.

What last happened to the process follows in parentheses — `exited` with its
code, `signaled` with its signal, `timeout-start`, `timeout-stop`,
`crash-limit` or `exec-failed`. Nothing is shown there when the last thing that
happened was a clean success.

The origin of the transition is always appended as `by <who>`:

```
Status : enabled, down (exited 1) since 2min 5s by user
```

`who` answers *who wanted this state*, not who last poked the process:

- **`user`** — a state you asked for, with [start](66-start.html) or [stop](66-stop.html).
- **`boot`** — the boot procedure brought it up.
- **`shutdown`** — the shutdown procedure brought it down.
- **`event`** — an [event](66-eventd.html) reactor triggered the transition.
- **`self`** — the supervisor acted on its own.

A service killed with [66 signal](66-signal.html) reports `self`: a signal does
not ask the service to go down, so the death is intrinsic and the supervisor
respawns it on its own initiative. Only a command that flips the *wanted* state
owns the transition.

Everything the line glues together also stands alone as its own field, which is
what a script wants: `-f enabled` yields `yes` or `no`, `-f pid` the bare pid,
`-f type` the type that otherwise rides with the name. Under `-n` every field
yields its own value, unglued and with raw seconds:

```
$ 66 status -nf enabled dbus
yes
$ 66 status -nf pid dbus
620
$ 66 status -nf status dbus
up (pid 620) since 32340 by boot
```

For the raw runtime record behind these states — the exact timestamps, the exit
code and the crash budget — see [66 runstate](66-runstate.html). For the granular
internal flags, see [66 state](66-state.html).

## Usage examples

Displays all information of service `foo`

```
66 status foo
```

Only displays the field `name` and `status` of service `foo`

```
66 status -f name,status foo
```

Also, do not display the name of the field `name` and `status` of service `foo`

```
66 status -nf name,status foo
```

Displays the state of `foo` without its log

```
66 status -f name,status,enabled foo
```

Reads the whole log of the service `foo` — `status` only ever shows its last few lines

```
66 log foo
```

In a script you can do

```
#!/bin/sh

service="${1}"
type=$(66 status -nf type ${service})

if [ ${type} = "classic" ]; then
    echo ${service} is a classic service
elif [ ${type} = "module" ]; then
    echo ${service} is a module service
elif [ ${type} = "oneshot" ]; then
    echo ${service} is a oneshot service
fi
```

Displays information of the service using the graph mode

```
66 status -g dbus

Name         : dbus ( classic )
Status       : enabled, up (pid 620) since 8h 59min by boot
Description  : dbus system daemon
Source       : /usr/share/66/service/dbus
Tree         : global
Dependencies : \
               └─dbus-log (pid=615, state=Enabled, type=classic, tree=global)
Required by  : \
               └─consolekit (pid=632, state=Enabled, type=classic, tree=global)
Log          : dbus-log - '66 log dbus' for more
2026-07-17 12:27:25.136118430  dbus-daemon[626]: [system] Activating service name='org.freedesktop.PolicyKit1' requested by ':1.0'
2026-07-17 12:27:25.149366699  dbus-daemon[626]: [system] Successfully activated service 'org.freedesktop.PolicyKit1'
2026-07-17 12:28:12.083741993  dbus[620]: Unknown username "colord" in message bus configuration file
2026-07-17 12:28:12.099542827  dbus-daemon[620]: [system] Activating service name='org.freedesktop.PolicyKit1' requested by ':1.0'
2026-07-17 12:28:12.113367939  dbus-daemon[620]: [system] Successfully activated service 'org.freedesktop.PolicyKit1'
```
