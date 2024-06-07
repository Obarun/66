title: The 66 Suite: status
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# status

This command displays information about service.

## Interface

```
status [ -h ] [ -n ] [ -o name,intree,status,... ] [ -g ] [ -d depth ] [ -r ] [ -p nline ] service
```

By default the dependency graph is rendered in the [start](66-start.html) order of execution. You can reverse the rendered order, meaning the [stop](66-stop.html) execution, with the `-r` option.

Without specifying the `-o`, all fields are displayed.

If no *service* is specified, it displays all services from all trees. This is a useful way to quickly get an overview of the entire service system. In that case, `-g` is implied and `-p`, `-o` and `-n` options have no effects.

## Options

- **-h**: prints this help.

- **-n**: do not display the field name(s) specified. Combining this options with the `-o` facilitates scripting usage.

- **-o**: comma separated list of fields to display.

- **-g**: shows the dependency list of the *service* as a hierarchical graph instead of a list.

- **-d** *depth*: limits the depth of the dependency list visualisation; default is 1. This implies **-g** option.

- **-r**: shows the dependency list of *services* in reverse mode.

- **-p** *nline*: prints the *nline* last lines from the log file of the *service*. Default is 20.

## Valid fields for -o options

- **name**: displays the name.
- **version**: displays the version of the service.
- **intree**: displays the service's tree name.
- **status**: displays the status.
- **type**: displays the service type.
- **description**: displays the description.
- **partof**: displays the module name for services part of that module.
- **notify**: displays the number of the fd for readiness notification.
- **maxdeath**: displays the number of maximum death.
- **earlier**: displays if service is an earlier one.
- **source**: displays the source of the service's [frontend](66-frontend.html) file.
- **live**: displays the service's live directory.
- **depends**: displays the service's dependencies.
- **requiredby**: displays the service(s) which depends on service.
- **contents**: displays services within module.
- **optsdepends**: displays the service's optional dependencies. Also displays the name of the associated *tree* of the optional dependencies after the `colon(:)` mark if any.
- **start**: displays the service's start script.
- **stop**: displays the service's stop script.
- **envat**: displays the source of the environment file.
- **envfile**: displays the contents of the environment file.
- **logname**: displays the logger's name.
- **logdst**: displays the logger's destination.
- **logfile**: displays the contents of the log file.

## Usage examples

Displays all information of service `foo`

```
66 status foo
```

Only displays the field `name` and `status` of service `foo`

```
66 status -o name,status foo
```

Also, do not display the name of the field `name` and `status` of service `foo`

```
66 status -no name,status foo
```

Only displays the contents of the log file of the service `foo`

```
66 status -o logfile foo
```

Also, displays the last 100 lines of the log file of the service `foo`

```
66 status -o logfile -p100 foo
```

In a script you can do

```
#!/bin/sh

service="${1}"
type=$(66 status -no type ${service})

if [ ${type} = "classic" ]; then
    echo ${service} is a classic service
else if [ ${type} = "module" ]; then
    echo ${service} is a module service
else if [ ${type} = "oneshot" ]; then
    echo ${service} is a oneshot service
fi
```

Displays information of the service using the graph mode

```
66 status -g dbus

Name                  : dbus
Version               : 0.0.1
In tree               : global
Status                : enabled, up (pid 731) 34829 seconds, ready 34829 seconds
Type                  : classic
Description           : dbus system daemon
Part of               : None
Notify                : 4
Max death             : 3
Earlier               : 0
Source                : /etc/66/service/dbus
Live                  : /run/66/scandir/0/dbus
Dependencies          : \
                        └─dbus-log (pid=723, state=Enabled, type=classic, tree=global)
Required by           : \
                        ├─networkmanager (pid=747, state=Enabled, type=classic, tree=global)
                        ├─boot-user@oblive (pid=up, state=Enabled, type=module, tree=session)
                        └─consolekit (pid=746, state=Enabled, type=classic, tree=global)
Contents              : \
                        └─None
Optional dependencies : None
Start script          :
                        #!/usr/bin/execlineb -P
                        fdmove -c 2 1
                        execl-envfile -v4 /etc/66/conf/dbus/version
                            execl-toc -S ${socket_name} -m 0755
                            foreground {
                                execl-toc -d /var/lib/dbus
                                dbus-uuidgen --ensure
                            }
                            execl-cmdline -s { dbus-daemon ${cmd_args} }
Stop script           :
                        #!/usr/bin/execlineb -P
                        fdmove -c 2 1
                        execl-envfile -v4 /etc/66/conf/dbus/version
                         s6-rmrf ${socket_name}
Environment source    : /etc/66/conf/dbus/0.0.1
Environment file      : environment variables from: /etc/66/conf/dbus/0.0.1/.dbus
                        cmd_args=!--system --print-pid=4 --nofork --nopidfile --address=unix:path=${socket_name}
                        socket_name=!/run/dbus/system_bus_socket

                        environment variables from: /etc/66/conf/dbus/0.0.1/dbus
                        cmd_args=!--system --print-pid=4 --nofork --nopidfile --address=unix:path=${socket_name}
                        socket_name=!/run/dbus/system_bus_socket

Log name              : dbus-log
Log destination       : /var/log/66/dbus
Log file              :
dbus-daemon[731]: [system] Activating service name='org.freedesktop.nm_dispatcher' requested by ':1.2' (uid=0 pid=747 comm="NetworkManager -d") (using servicehelper)
dbus-daemon[731]: [system] Successfully activated service 'org.freedesktop.nm_dispatcher'
dbus-daemon[731]: [system] Activating service name='org.freedesktop.nm_dispatcher' requested by ':1.2' (uid=0 pid=747 comm="NetworkManager -d") (using servicehelper)
dbus-daemon[731]: [system] Successfully activated service 'org.freedesktop.nm_dispatcher'
dbus-daemon[731]: [system] Activating service name='org.freedesktop.nm_dispatcher' requested by ':1.2' (uid=0 pid=747 comm="NetworkManager -d") (using servicehelper)
dbus-daemon[731]: [system] Successfully activated service 'org.freedesktop.nm_dispatcher'
dbus-daemon[731]: [system] Activating service name='org.freedesktop.nm_dispatcher' requested by ':1.2' (uid=0 pid=747 comm="NetworkManager -d") (using servicehelper)
dbus-daemon[731]: [system] Successfully activated service 'org.freedesktop.nm_dispatcher'
dbus-daemon[731]: [system] Activating service name='org.freedesktop.nm_dispatcher' requested by ':1.2' (uid=0 pid=747 comm="NetworkManager -d") (using servicehelper)
dbus-daemon[731]: [system] Successfully activated service 'org.freedesktop.nm_dispatcher'
dbus-daemon[731]: [system] Activating service name='org.freedesktop.nm_dispatcher' requested by ':1.2' (uid=0 pid=747 comm="NetworkManager -d") (using servicehelper)
dbus-daemon[731]: [system] Successfully activated service 'org.freedesktop.nm_dispatcher'
dbus-daemon[731]: [system] Activating service name='org.freedesktop.nm_dispatcher' requested by ':1.2' (uid=0 pid=747 comm="NetworkManager -d") (using servicehelper)
dbus-daemon[731]: [system] Successfully activated service 'org.freedesktop.nm_dispatcher'
dbus-daemon[731]: [system] Activating service name='org.freedesktop.nm_dispatcher' requested by ':1.2' (uid=0 pid=747 comm="NetworkManager -d") (using servicehelper)
dbus-daemon[731]: [system] Successfully activated service 'org.freedesktop.nm_dispatcher'
dbus-daemon[731]: [system] Activating service name='org.freedesktop.nm_dispatcher' requested by ':1.2' (uid=0 pid=747 comm="NetworkManager -d") (using servicehelper)
dbus-daemon[731]: [system] Successfully activated service 'org.freedesktop.nm_dispatcher'
dbus-daemon[731]: [system] Activating service name='org.freedesktop.nm_dispatcher' requested by ':1.2' (uid=0 pid=747 comm="NetworkManager -d") (using servicehelper)
dbus-daemon[731]: [system] Successfully activated service 'org.freedesktop.nm_dispatcher'
```
