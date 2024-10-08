title: The 66 Suite: scandir
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# scandir

Handles the *scandir* for a given user. Designed to be either root or a branch of the supervision tree(nested *scandir*).

## Interface

```
scandir [ -h ] [ -o owner ] create|start|stop|remove|reconfigure|check|quit|abort|nuke|annihilate|zombies [<subcommand options>]
```

This program creates, removes or sends a signal to a *scandir* (directory containing a collection of s6‑supervise processes) for the current owner of the proccess depending on the provided options.

When the `start` subcommand is invoked, this command launches the [s6-svscan](https://skarnet.org/software/s6/s6-svscan.html), responsible for supervising the [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) program, where [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) monitors a single service.

## Options

- **-h**: prints this help.

- **-o** *owner*: handles the *scandir* for the given *owner*. Only the root user can use this option. Note that *owner* can be any valid user on the system. However, the given user must have sufficient permissions to create the necessary directories at its location. That is `%%livedir%%` by default or the resulting path provided by the `66  ‑l` option.

## Subcommands

- **create**: create a *scandir*.
- **start**: start a *scandir*.
- **stop**: stop a running *scandir*.
- **remove**: remove a *scandir*.
- **reconfigure**: reconfigure a running *scandir*.
- **check**: check a running *scandir*.
- **quit**: quit a running *scandir*.
- **abort**: abort a running *scandir*.
- **nuke**: nuke a running *scandir*.
- **annihilate**: annihilate a running *scandir*.
- **zombies**: destroy zombies from a running *scandir*.

## Usage examples

Creates a *scandir* for the *owner* of the process
```
66 scandir create
```

Creates a *scandir* for the *owner* `owner`
```
66 scandir -o owner create
```

Creates a *scandir* for a boot process
```
66 scandir create -b
```

Creates a *scandir* for the boot process using the specific account `logaccount` for the logger
```
66 scandir create -b -L logaccount
```

Creates (if doesn't exist yet) and starts a *scandir* for the boot process within a container adding an extra environment directory called `/myenvdir`.
```
66 scandir start -B -e /myenvdir
```

Stops an already running *scandir*
```
66 scandir stop
```

### create

This subcommand create a *scandir*.

#### Interface

```
scandir create [ -h ] [ -b|B ] [ -c ] [ -L log_user ] [ -s skel ]
```

This command create the necessary directory at `%%livedir%%`. If the *scandir* already exist for the given user it will prevent its creation. You *must* remove it first if you don't want to keep it.

Various files and directories is created at `%%livedir%%`. Refers to [deeper understanding](66-deeper.html) documentation for futhers information.

#### Options

- **-h**: print this help.
- **-b**: create scandir for a boot process. Only the root user can use this option. It is not meant to be used directly even with root. [66 boot](66-boot.html) calls it during the boot process.
- **-B**: create scandir for a boot process inside a container. This option modifies some behaviors:

    The ultimate output fallback (i.e. the place where error messages go when nothing catches them, e.g. the error messages from the catch-all logger and the [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) process managing the catch-all logger) is not `/dev/console`, but the descriptor that was init's standard error.
    Stopping the container with reboot will make the container's init program report being killed by a `SIGHUP`. Stopping it with [66 poweroff](66-poweroff.html) will make it report being killed by a `SIGINT`. This is according to the reboot(2) specification.
    Stopping the container with [66 halt](66-halt.html), however, is different. It will make the container's pid 1 read a number in the `/run/66/container/\<owner\>/halt` file which contents the variable `EXITCODE`, and exit with the code it has read. (Default is 0.) This means that in order to run a command in a container managed by [66 boot](66-boot.html) and exit the container when the command dies while reporting the exit code to its parent, [66 boot](66-boot.html) use the `/etc/66/rc.init.container` file instead of the `/etc/66/rc.init` file. This file should be modified to launch the command that you want to start inside this container. Then you need to stop that container calling [66 halt](66-halt.html).
    All the running services will be killed, all the zombies will be reaped, and the container will exit with the required exit code.

- **-c**: do not catch logs. On a non-containerized system, that means that all the logs from the *scandir* will go to `/dev/console`, and that `/dev/console` will also be the default `stdout` and `stderr` for services running under the supervision tree: use of this option is discouraged. On a containerized system (when paired with the `-B` option), it simply means that these outputs go to the default `stdout` and `stderr` given to the container's init - this should generally not be the default, but might be useful in some cases.

- **-L** *log_user*: run catch-all logger as *log_user* user. Default is `%%s6log_user%%`. The default can also be changed at compile-time by passing the `‑‑with‑s6‑log‑user=user` option to `./configure`.

- **-s** *skel*: use *skel* as skeleton directory. Directory containing *skeleton* files. This option is not meant to be used directly even with root. [66 boot](66-boot.html) calls it during the boot process. Default is `%%skel%%`.

#### Usage examples

Creates a scandir for the *owner* of the process
```
66 scandir create
```

Creates a scandir for the boot process using the specific account `logaccount` for the logger
```
66 scandir create -b -L logaccount
```

### start

This subcommand starts a *scandir* and, if it doesn't exist yet, it possibly creates it .

#### Interface

```
scandir start [ -h ] [ -d notif ] [ -s rescan ] [ -e environment ] [ -b|B ]
```

The *scandir* is created if it wasn't made previously, but you don't a fine-grained control over its creation as you do with the `create` subcommand.

#### Options

- **-h**: prints this help.

- **-d** *notif*: notify readiness on file descriptor notif. When *scandir* is ready to accept signal, it will write a newline to *notif*. *notif* **cannot be** lesser than `3`. By default, no notification is sent. If **-b** is set, this option have no effects.

- **-t** *rescan*: perform a scan every *rescan* milliseconds. If *rescan* is set to 0 (the default), automatic scans are never performed after the first one and [s6‑svscan](https://skarnet.org/software/s6/s6-svscan.html) will only detect new services by issuing either [scandir reload](66-scandir.html#reload) or [scandir check](66-scandir.html#check). It is **strongly** discouraged to set *rescan* to a positive value under `500`.

- **-e** *environment*: an absolute path. Merge the current environment variables with variables found in this directory before starting the *scandir*. Any file in environment not beginning with a dot and not containing the `=` character will be read and parsed. Each services started within the *scandir* will inherit of the `key=value` pair define within *environment*. By default, *66* import the %%environment_adm%% environment directory by default for the root user and the %%environment_user%% directory for the regular user. Although this can be changed at compile time by passing the `--with-sysadmin-environment=DIR `, `--with-user-environment=DIR` for root and regular user respectively. In case of same `key=value` pair, the environment directory define with the `-e` take precedence. Also, see [Environment](#environment) for further information about the syntax and the limitations.

#### Usage examples

Starts a scandir with notify readiness mechanism on file descriptor `3`
```
66 scandir start -d 3
```

Starts a scandir for the *owner* `owner`
```
66 scandir -o owner start
```

### stop

This command stops a running *scandir*.

#### Interface

```
scandir stop [ -h ]
```

This command stops the *scandir* sending a `SIGTERM` to all the [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) processes supervising a service and a `SIGHUP` to all the [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) processes supervising a logger, then exec into its finish procedure. This means that services will be brought down but loggers will exit naturally on `EOF`, and [s6-svscan](https://skarnet.org/software/s6/s6-svscan.html) will wait for them to exit before exec'ing into `.s6-svscan/finish` or exiting itself: it's a clean shutdown with no loss of logs.

#### Options

- **-h**: prints this help.

#### Usage examples

Stop an already running *scandir*
```
66 scandir stop
```

Stop an already running *scandir* for the owner `owner`
```
66 scandir -o owner stop
```

### remove

This command remove a *scandir* from the *live* directory.

#### Interface

```
scandir remove [ -h ]
```

The *scandir* **must** first be stopped with [scandir stop](#stop) subcommand or similar subcommand to be able to remove it.

Certain directories within the scandir will not be removed. Specifically, `%%livedir%%/log`, `%%livedir%%/scandir`, and `%%livedir%%/state` remain intact, whereas all `UID` subdirectories are deleted. Refers to [deeper understanding](66-deeper.html) for futhers information.

#### Options

- **-h**: prints this help.

#### Usage examples

Removes a *scandir*
```
66 scandir remove
```

Removes a *scandir* for the owner `owner`
```
66 scandir -o owner scandir
```

### reconfigure

This command reconfigure a running *scandir*.

#### Interface

```
scandir reconfigure [ -h ]
```

[s6-svscan](https://skarnet.org/software/s6/s6-svscan.html) will perform a scan, and destroy inactive services.

#### Options

- **-h**: prints this help.

#### Usage examples

Reconfigures a running *scandir*.

```
66 scandir reconfigure
```

Reconfigures a *scandir* for the owner `owner`

```
66 scandir -o owner reconfigure
```

### check

This command check the *scandir* for services.

#### Interface

```
scandir check [ -h ]
```

[s6-svscan](https://skarnet.org/software/s6/s6-svscan.html) will immediately perform a scan of *scandir* to check for services.

#### Options

- **-h**: prints this help.

#### Usage examples

Checks a *scandir*

```
66 scandir check
```

Checks a *scandir* for the owner `owner`

```
66 scandir -o owner check
```

### quit

Quits a running *scandir*.

#### Interface

```
scandir quit [ -h ]
```

[s6-svscan](https://skarnet.org/software/s6/s6-svscan.html) will send all its [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) processes a `SIGTERM`, then exec into its finish procedure. This is different from `stop` subcommand in that services and loggers will be forcibly killed, so the quit procedure may be faster but in-flight logs may be lost.

#### Options

- **-h**: prints this help.

#### Usage examples

Quits a *scandir* running *scandir*.

```
66 scandir quit
```

Quits a *scandir* for the owner `owner`

```
66 scandir -o owner quit
```

### abort

This command abort a running *scandir*.

#### Interface

```
scandir abort [ -h ]
```

[s6-svscan](https://skarnet.org/software/s6/s6-svscan.html) will exec into its finishing procedure. It will not kill any of the maintained [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) processes.

#### Options

- **-h**: prints this help.

#### Usage examples

Aborts a *scandir*

```
66 scandir abort
```

Aborts a *scandir* for the owner `owner`

```
66 scandir -o owner abort
```

### nuke

Kill all the [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) processes.

#### Interface

```
scandir nuke [ -h ]
```
[s6-svscan](https://skarnet.org/software/s6/s6-svscan.html) kill all the [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) processes it has launched but that did not match a service directory last time *scandir* was scanned, i.e. it prunes the supervision tree so that it matches exactly what was in *scandir* at the time of the last scan. A `SIGTERM` is sent to the [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) processes supervising services and a `SIGHUP` is sent to the [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) processes supervising loggers.

#### Options

- **-h**: prints this help.

#### Usage examples

Kill s6-supervise processes from a *scandir*

```
66 scandir nuke
```

Kill s6-supervise processes from a *scandir* for the owner `owner`

```
66 scandir -o owner nuke
```

### annihilate

Annihilates a running *scandir*.

#### Interface

```
scandir annihilate [ -h ]
```

Does the same thing as [nuke](#nuke), except that `SIGTERM` is sent to all the relevant [s6-supervise](https://skarnet.org/software/s6/s6-supervise.html) processes, even if they are supervising loggers. This is not recommended in a situation where you do not need to tear down the supervision tree.

#### Options

- **-h**: prints this help.

#### Usage examples

Annihilates a *scandir*

```
66 scandir annihilate
```

Annihilates a *scandir* for the owner `owner`

```
66 scandir -o owner annihilate
```

### zombies

Destroy zombies from a running *scandir*.

#### Interface

```
scandir zombies [ -h ]
```

Immediately triggers s6-svscan's reaper mechanism.

#### Options

- **-h**: prints this help.

#### Usage examples

Removes zombies from a *scandir*

```
66 scandir zombies
```

Removes zombies from a *scandir* for the owner `owner`

```
66 scandir -o owner zombies
```

## Boot specification

The **-b**, **-B**, **-c** and **-s** option are called by [66 boot](66-boot.html). **‑b** and **-B** will create `.s6‑svscan` control files (see [s6‑svscan](https://skarnet.org/software/s6/s6-svscan.html) interface documentation) specifically for stage1 (PID1 process). This special *scandir* is controlled by [66 halt](66-halt.html), [66 poweroff](66-poweroff.html) and  [66 reboot](66-reboot.html) command. The [66-shutdownd](66‑shutdownd.html) daemon which controls the shutdown request will be created automatically at the correct location. Further this specific task needs to read the skeleton file `init.conf` containing the *live* directory location which is the purpose of the **‑s** option.

The *live* directory for the boot process requires writable directories and an executable filesystem. In order to accommodate for read‑only root filesystems there needs to be a tmpfs mounted before [s6‑svscan](https://skarnet.org/software/s6/s6-svscan.html) can be run.

## Environment

An Environment directory is a directory containing files with lines of pairs with the syntax being: `key = value`. The directory is parsed by ascending alphabetical order. If there are duplicate `key=value` pairs, the pair found in the last file takes precedence.

Whitespace is permitted before and after *key*, and before or after *value*.

Empty lines, or lines containing only whitespace, are ignored. Lines beginning with `#` (possibly after some whitespace) are ignored (and typically used for comments). Leading and trailing whitespace is stripped from values; but a *value* can be double-quoted, which allows for inclusion of leading and trailing whitespace.

Escaping double-quoted can be done with backslash `\`. For instance,

```
cmd_args=-g \"daemon off;\"
```

C escapes, including hexadecimal and octal sequences, are supported in quoted values. Unicode codepoint sequences are not supported.

If *value* is empty, *key* is still added to the environment, with an empty value.

Reusing the same variable or variable from the actual environment is allowed. In such case, variable name **must be** between `${}` to get it value. For intance, an environment file can be declared

```
    PATH=/usr/local/bin:${PATH}
    socket_name=sname
    socket_dir=dname
    socket=${socket_dir}/${socket_name}
```

The order of `key=value` pair declaration **do not** matter

```
    PATH=/usr/local/bin:${PATH}
    socket=${socket_dir}/${socket_name}
    socket_name=sname
    socket_dir=dname
```

A variable calling itself is **only** allowed if the `key` name can be found at the environment of the current process. If the key of the `key=value` cannot not be found it left the pair as it. For intance,

```
    PATH=/usr/local/bin:${PATH}
```

will only works if `PATH` is already define at the current environment. If not the result will literally be `PATH=/usr/local/bin:${PATH}`.

### Limits

An environment directory ***can not*** exceed more than `20` files. Each file can not contain more than `8191` bytes or more than `50` `key=value` pairs.