title: The 66 Suite: 66-inservice
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# 66-inservice

This command displays information about services.

## Interface

```
    66-inservice [ -h ] [ -z ] [ -v verbosity ] [ -n ] [ -o name,intree,status,... ] [ -g ] [ -d depth ] [ -r ] [ -t tree ] [ -p nline ] service
```

## Exit codes

- *0* success
- *100* wrong usage
- *111* system call failed

## Options

- **-h** : prints this help.

- **-z** : use color.

- **-v** *verbosity* : increases/decreases the verbosity of the command.
    * *1* : only print error messages. This is the default.
    * *2* : also print warning messages.
    * *3* : also print tracing messages.
    * *4* : also print debugging messages.

- **-n** : do not display the field name(s) specified.

- **-o** : comma separated list of fields to display. If this option is not passed, *66-inservice* will display all fields.

- **-g** : shows the dependency list of the *service* as a hierarchical graph instead of a list.

- **-d** *depth* : limits the depth of the dependency list visualisation; default is 1. This implies **-g** option.

- **-r** : shows the dependency list of *services* in reverse mode.

- **-t** *tree* : only searches the *service* at the specified *tree*, when the same service may be enabled in more trees.

- **-p** *nline* : prints the *nline* last lines from the log file of the *service*. Default is 20.

## Valid fields for -o options

- *name* : displays the name of the *service*.
- *intree* : displays the *service*'s tree name.
- *status* : displays the status.
- *type* : displays the *service* type.
- *description* : displays the description.
- *source* : displays the source of the *service*'s [frontend](frontend.html) file.
- *live* : displays the *service*'s live directory.
- *depends* : displays the *service*'s dependencies.
- *extdepends* : displays the *service*'s external dependencies.
- *optsdepends* : displays the *service*'s optional dependencies.
- *start* : displays the *service*'s start script.
- *stop* : displays the *service*'s stop script.
- *envat* : displays the source of the environment file.
- *envfile* : displays the contents of the environment file.
- *logname* : displays the logger's name.
- *logdst* : displays the logger's destination.
- *logfile* : displays the contents of the log file.

## Command and output examples

The command `66-inservice 00` run as root user on [Obarun](https://web.obarun.org)'s default system, displays the following, where *00* is a service contained in the tree boot:

```
    Name                  : 00
    In tree               : boot
    Status                : enabled, nothing to display
    Type                  : bundle
    Description           : Set the hostname and mount filesystem
    Source                : %%service_system%%/boot/mount/00
    Live                  : %%livedir%%/tree/0/boot/servicedirs/00
    Dependencies          : system-hostname  mount-run  populate-run  mount-tmp  populate-tmp  mount-proc  mount-sys
                            populate-sys  mount-dev  mount-pts  mount-shm  populate-dev  mount-cgroups
    External dependencies : None
    Optional dependencies : None
    Start script          : None
    Stop script           : None
    Environment source    : None
    Environment file      : None
    Log name              : None
    Log destination       : None
    Log file              : None
```

***Note***: the *Optional dependencies* and *External dependencies* also displays the name of the tree where the service is currently enabled after the `colon(:)` mark if any:

```
    External dependencies : dbus-session@obarun:base gvfsd:desktop
    Optional dependencies : picom@obarun:desktop
```

By default the dependency graph is rendered in the order of execution. In this example the `oneshot` system-hostname is the first executed service and `oneshot` *mount-cgroups* is the last one when it finishes. You can reverse the rendered order with the **-r** option.

You can display the *status* and *depends on* field and only these fields of a *service* using the command `66-inservice -o status,depends -g <service>` connmand where the dependency list is diplayed as a graph:

```
    Status             : enabled, up (pid 938) 34652 seconds
    Dependencies       : /
                         ├─(933,Enabled,longrun) dbus
                         └─(929,Enabled,longrun) connmand-log
```

You can display the status,log file and log destination and only these fields of a service using the command `66-inservice -o status,logdst,logfile -g dbus`:

```
    Status             : enabled, up (pid 933) 34852 seconds, ready 34852 seconds
    Log destination    : %%system_log%%/dbus
    Log file           :
    2019-10-03 16:16:05.246991500  dbus-daemon[933]: [system] Rejected send message, 4 matched rules; type="method_call", sender=":1.3" (uid=1000 pid=1226 comm="connman-gtk --tray ") interface="net.connman.Technology" member="Scan" error name="(unset)" requested_reply="0" destination=":1.0" (uid=0 pid=938 comm="connmand -n --nobacktrace --nodnsproxy ")
    2019-10-03 16:16:35.256146500  dbus-daemon[933]: [system] Rejected send message, 4 matched rules; type="method_call", sender=":1.3" (uid=1000 pid=1226 comm="connman-gtk --tray ") interface="net.connman.Technology" member="Scan" error name="(unset)" requested_reply="0" destination=":1.0" (uid=0 pid=938 comm="connmand -n --nobacktrace --nodnsproxy ")
```
