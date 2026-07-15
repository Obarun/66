# The frontend service file

A supervised service is, under the hood, made of several different files whose relationships can be complex to understand and manage. The frontend service file of the `66` program lets you describe all the different kinds of services — such as [classic](#type) and [module](66-module-creation.html) services — in a centralized manner and in one single location, and `66` generates everything its native supervision needs from it.

By default `66` program expects to find service files in `%%service_system%%` and `%%service_adm%%` for root user, `%%service_system%%/user` and `%%service_adm%%/user` for regular accounts. For regular accounts, `$HOME/%%service_user%%` will take priority over the previous ones. Although this can be changed at compile time by passing the `-D system-service-dir=DIR`, `-D sysadmin-service-dir=DIR` and `-D user-service-dir=DIR` option to `meson setup`.

The frontend service file has a format of `INI` with a specific syntax on the key field. The name of the file usually corresponds to the name of the daemon and does not have any extension or prefix.

The file is made of *sections* which can contain one or more `key = value` pairs where the *key* name can contain special characters like `-` (hyphen) or `_` (low line) except the character `@` (commercial at) which is reserved.

You can find a prototype with all valid section and all valid `key=value` pair at the end of this [document](#prototype-of-a-frontend-file).

### File names examples

```
%%service_system%%/dhcpcd
%%service_system%%/very_long_name_which_make_no_sense
```

### File content example

```
[Main]
Type = classic
Description = "ntpd daemon"

[Start]
Execute = (
    foreground { mkdir -p  -m 0755 ${RUNDIR} }
    execl-cmdline -s { ntpd ${CMD_ARGS} }
)

[Environment]
RUNDIR=!/run/openntpd
CMD_ARGS=!-d -s
```

The parser will **not** accept an empty value. If a *key* is set then the *value* **can not** be empty. Comments are allowed using the number sign `#`. Empty lines are also allowed.

*Key* names are **case sensitive** and can not be modified. Most names should be specific enough to avoid confusion.

## Sections

All sections need to be declared with the name written between square brackets `[]` and **must begin** with an uppercase followed by lowercase letters **only**. This means that special characters and numbers are not allowed in the name of a section.

The `[Main]` section **must be** declared first.

The frontend service file allows the following section names:

- [[Main]](#section-main)
- [[Start]](#section-start)
- [[Stop]](#section-stop)
- [[Logger]](#section-logger)
- [[Environment]](#section-environment)
- [[Regex]](#section-regex)
- [[Execute]](#section-execute)
- [[Event]](#section-event)

Although a section can be mandatory not all of its key fields must be necessarily so.

### Syntax legend

The *value* of a *key* is parsed in a specific format depending on the key. The following is a break down of how to write these syntaxes.

#### *inline*

An inline *value*. **Must** be on the same line with its corresponding *key*.

* Valid syntax:

    ````
    Type = classic

    Type=classic
    ````

* **(!)** Invalid syntax:

    ````
    Type=
    classic
    ````
#### *quotes*

A *value* between double-quotes. **Must** be on the same line with its corresponding *key*.

* Valid syntax:

    ````
    Description = "some awesome description"

    Description="some awesome description"
    ````

* **(!)** Invalid syntax:

    ````
    Description=
    "some awesome description"

    Description = "line break inside a double-quote
    is not allowed"
    ````

#### *brackets*

Multiple *values* between parentheses `()`. Values need to be separated with a space. A line break can be used instead.

* Valid syntax:

    ````
    Depends = ( fooA fooB fooC )

    Depends=(fooA fooB fooC)

    Depends=(
    fooA
    fooB
    fooC
    )

    Depends=
    (
    fooA
    fooB
    fooC
    )
    ````

* **(!)** Invalid syntax:

    ````
    Depends = (fooAfooBfooC)
    ````

#### *uint*

A positive whole number. **Must** be on the same line with its corresponding *key*.

* Valid syntax:

    ````
    Notify = 3

    Notify=3
    ````

* **(!)** Invalid syntax:

    ````
    Notify=
    3
    ````

#### *path*

An absolute path beginning with a forward slash `/`. **Must** be on the same line with its corresponding *key*.

* Valid syntax:

    ````
    ChangeDirectory = /etc/66

    ChangeDirectory=/etc/66
    ````

* **(!)** Invalid syntax:

    ````
    ChangeDirectory=/a/very/
    long/path
    ````

#### *pair*

Same as [*inline*](#inline).

* Valid syntax:

    ````
    MYKEY = MYVALUE

    anotherkey=anothervalue

    anotherkey=where_value=/can_contain/equal/Character
    ````

* **(!)** Invalid syntax:

    ````
    MYKEY=
    MYVALUE
    ````

#### *colon*

A value between double colons followed by a *pair* syntax. **Must** be one by line.

* Valid syntax:

    ````
    ::key=value

    :filename:key=value
    ````

* **(!)** Invalid syntax:

    ````
    ::MYKEY=
    MYVALUE

    ::
    MYKEY=MYVALUE

    ::key=value :filename:anotherkey=anothervalue
    ````

#### *simple-colon*

A values separated by a colon. **Must** be on the same line with its corresponding *key*.

* Valid syntax:

    ````
    RunAs = 1000:19
    ````

* **(!)** Invalid syntax:

    ````
    RunAs = 1000:
    19
    ````

#### *boolean*

A value specifying a true state for the key. **Must** be on the same line with its corresponding *key*. If the key is not defined, it defaults to `false`.

* **Valid syntax**:

    ````
    BlockPrivileges = true
    BlockPrivileges = True
    BlockPrivileges = TRUE
    BlockPrivileges = 1
    BlockPrivileges = false
    BlockPrivileges = False
    BlockPrivileges = FALSE
    BlockPrivileges = 0
    ````

* **(!)** **Invalid syntax**:

    ````
    BlockPrivileges =
    true
    BlockPrivileges =
    ````

* note: For code simplicity and rapidity, setting e.g. `key = T` is strictly equivalent to `key = True` or `key = TRUE` as the parser only checks the first letter of the string value.

### Section [Main]

This section is *mandatory*. (!)

#### Type

**Source Snippet**:
```ini
Type = classic
```

Defines the service type. Determines how **66** orchestrates startup and supervision.

* mandatory: yes (!)

* syntax: [inline](#inline)

* valid values :

    * classic : Standard supervised service. Runs continuously and is automatically restarted if it crashes.
    * oneshot : Executes once and does not restart. Suitable for initialization tasks.
    * module : Configurable set of different type of service; integrates with the [`[Regex]`](#section-regex) section for file and directory transformations.
    * event : A non-supervised **event source** for the [event system](66-event.html). It runs no process and has no `[Start]` section; its whole configuration lives in `[Main]` and is selected by [`EventType`](#eventtype). A `classic`/`oneshot`/`module` service becomes an event **reactor** instead by adding an [`[Event]`](#section-event) section — it does not use this value.

#### Version

**Source Snippet**:
```ini
Version = 0.1.0
```

Specifies the semantic version of the service. This helps track updates and compatibility. If not specified, defaults to the actual installed version of *66*.

* mandatory: no

* syntax: [inline](#inline)

* valid values:

    * Any valid version with number, alphabetical, release or mixed components. See [A word about the Version key](#a-word-about-the-version-key) below.


#### Description

**Source Snippet**:
```ini
Description = "ntpd daemon"
```

Provides a concise, human-readable summary of the service’s purpose. Enclosed in double quotes. If not specified, defaults to "<service_name> service".

* mandatory: no

* syntax: [quote](#quotes)

* valid values:

    * Anything you want.

#### User

**Source Snippet**:
```ini
User = ( root )
```

Specifies the system user(s) list allowed to manage and operate the service. If not defined, defaults to the current process owner's username. *66* automatically distinguishes between user services and root services based on their installation paths. By default, only the specified users can start, stop, or interact with the service

* mandatory: no

* syntax: [brackets](#brackets)

* valid values :

    * Any valid user of the system. If you don't know in advance the name of the user who will deal with the service, you can use the term `user`. In that case every user of the system will be able to deal with the service. You can also use the `@U` identifier to be more specific.

    (!) Be aware that `root` is not automatically added for a user service. If you don't declare `root` in this field, you will not be able to use the service even with `root` privileges.

#### Depends

**Source Snippet**:
```ini
Depends = ( fooA fooB fooC )
```

Declares the mandatory service dependencies. Each listed service must start successfully before this service launches.

* mandatory: no

* syntax: [brackets](#brackets)

* valid values:

    * The name of any valid service.

    It is unnecessary to manually define chained sets of dependencies, see [66](66.html#handling-dependencies).

    A service can be commented out by placing the number sign `#` at the beginning of the name like this:

    ````
    Depends = ( fooA #fooB fooC )
    ````

#### RequiredBy

**Source Snippet**:
```ini
RequiredBy = ( fooX fooY )
```

Specifies reverse dependencies—services that depend on this service. Starting or enabling this service automatically updates those listed.

* mandatory: no

* syntax: [brackets](#brackets)

* valid values:

    * The name of any valid service.

    It is unnecessary to manually define chained sets of dependencies, see [66](66.html#handling-dependencies).

    A service can be commented out by placing the number sign `#` at the beginning of the name like this:

    ````
    RequiredBy = ( fooX #fooY )
    ````

#### OptsDepends

**Source Snippet**:
```ini
OptsDepends = ( fooA fooB )
```

Lists optional dependencies. **66** will enable the first available service from this list at startup.

* mandatory: no

* syntax: [brackets](#brackets)

* valid values:

    * The name of any valid service. A service declared as optional dependencies is not mandatory. The parser will look the corresponding service:
        - If enabled, it will warn the user and do nothing.
        - If not, it will try to find the corresponding frontend file.
            - If the frontend service file is found, it will enable it.
            - If it is not found, it will warn the user and do nothing.

    The order is *important* (!). The first service found will be used and the parse process of the field will be stopped. So, you can consider `OptsDepends` field as: "enable one on this service or none".

    A service can be commented out by placing the number sign `#` at the beginning of the name like this:

    ````
    OptsDepends = ( fooA #fooB fooC )
    ````

#### Options

**Source Snippet**:
```ini
Options = (log)
```

Configures optional behaviors for the service. Wrap options in parentheses for multiple entries.

* mandatory: no

* syntax: [brackets](#brackets)

* valid values:

    * log : automatically create a logger for the service. This is **default**. The logger will be created even if this options is not specified. If you want to avoid the creation of the logger, prefix the options with an exclamation mark:

        ````
        Options = ( !log )
        ````

        The behavior of the logger can be configured in the corresponding section—see [[Logger]](#section-logger).

#### Flags

**Source Snippet**:
```ini
Flags = (down earlier)
```

* mandatory: no

* syntax: [brackets](#brackets)

* valid values:

    * down: This will create the *down* file used by the supervisor. Once this file was created the default state of the service will be considered down, not up: the service will not automatically be started until it receives a [66 start](66-start.html) command. Without this file the default state of the service will be up and started automatically.
    * earlier: This set the service as an *earlier* service meaning starts the service as soon as the [scandir](66-scandir.html) is up.


#### Notify

**Source Snippet**:
```ini
Notify = 3
```

Enables readiness notification. Creates `notification-fd` containing the specified file descriptor number.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * Any valid number.

    This will create the file *notification-fd*. Once this file is created the service supports readiness notification. The value equals the number of the file descriptor that the service writes its readiness notification to — usually a dedicated descriptor such as `3` (or higher), matching the option your daemon uses to announce its readiness. Standard output (descriptor `1`) is normally unsuitable here, as it is redirected to the logger. When the service receives a signal and this file is present containing a valid descriptor number, [66](66.html) command will wait for the notification from the service and broadcast its readiness.

#### TimeoutStart / TimeoutStop (deprecated)

These keys have moved to the [[Start]](#section-start) and [[Stop]](#section-stop)
sections respectively. They are still accepted here for backward compatibility, but
emit a deprecation warning at parse time and will be removed from `[Main]` in a
future release — declare `TimeoutStart` in [[Start]](#section-start) and
`TimeoutStop` in [[Stop]](#section-stop) instead.


#### MaxDeath

**Source Snippet**:
```ini
MaxDeath = 5
```

Sets the crash budget: the number of times the service may die within a [MaxDeathInterval](#maxdeathinterval) window before the supervisor gives up and declares it *failed*, stopping any further automatic restart.

* mandatory: no

* syntax: [uint](#uint)

* valid value:

    * Any number from `0` to `16`. The default is `5`. A value of `0` disables the budget: the service is restarted indefinitely and is never declared *failed* for crash-looping.

    Only an actual *run-then-die* counts against the budget — the service did execute its `run` script, then exited or was signalled while still wanted up. A commanded [stop](66-stop.html) never counts, and a service that never managed to exec its `run` script (missing interpreter, unmounted filesystem, …) is reported as *exec failed* and retried with a progressive backoff without ever consuming the budget. Once the budget is exhausted the service stays *failed* until a new [start](66-start.html) resets the counter and relaunches it.

    Each automatic restart is throttled by a minimum delay of one second, so a crash-looping service consumes its budget at a rate of at most one death per second.

#### MaxDeathInterval

**Source Snippet**:
```ini
MaxDeathInterval = 30000
```

The length, in milliseconds, of the time window over which [MaxDeath](#maxdeath) crashes are counted.

* mandatory: no

* syntax: [uint](#uint)

* valid value:

    * Any valid number, in milliseconds. The default is `30000` (30 seconds).

    The window is measured on a monotonic clock and starts at the first counted death. If the service reaches `MaxDeath` deaths before the window elapses, it is declared *failed*. Otherwise — the window expires with fewer deaths — the window is re-armed: the next death starts a fresh window with the count reset to one. The measurement lives in volatile runtime state and is cleared on reboot. Because restarts are throttled to roughly one per second, exhausting the budget takes on the order of `MaxDeath` seconds; setting `MaxDeathInterval` much below that makes the *failed* state effectively unreachable through crash-looping alone.

#### DownSignal

**Source Snippet**:
```ini
DownSignal = SIGTERM
```

Specifies which signal to send when stopping or reloading the service.

* mandatory: no

* syntax: [inline](#inline)

* valid value:

    * The name or number of a signal.

    This will create the file *down-signal* which is used to kill the supervised process when a [reload](66-reload.html), [restart](66-restart.html) or [stop](66-stop.html) command is used. If the file does not exist `SIGTERM` will be used by default.

#### CopyFrom

**Source Snippet**:
```ini
CopyFrom = (./config /etc/default/service)
```

Verbatim copy directories and files on the fly to the main service destination. When dealing with directories, it copies all found files and directories recursively. In case of file, it copies it to the root of the service directory.

* mandatory: no

* syntax: [brackets](#brackets) with [path](#path) entries.

* valid values:

    * Any files or directories. It accepts *absolute* or *relative* path.

        ```
        CopyFrom = ( data
        ./.env
        /etc/resolv.conf)
        ```

    **Note**: `66` version must be higher than 0.3.0.1.

#### InTree

**Source Snippet**:
```ini
InTree = my-tree
```

Automatically activate the service within a named service tree. If a corresponding seed file exists, it will be applied.

* mandatory: no

* syntax: [inline](#inline)

* valid values :

    * Any name.

    The service will automatically be activated at the tree name set in the *InTree* key value.

    **Note**: If a corresponding [seed](66-tree.html#seed-files) file exist on your system, its will be used to create and configure the tree.

#### StdIn

**Source Snippet**:
```ini
StdIn = null
```

Controls standard I/O redirection for the standard input entries.

* mandatory: no

* syntax: [inline](#inline),[simple-colon](#simple-colon)

* valid values:

    * tty:/path/to/tty: Redirects Standard Input to the given tty specified by the path and try to become the controlling process of the terminal. The path must be absolute and exist. If the terminal is already being controlled by another process and the operation returns an EPERM failure, 66 will warn the user and continue its execution. If the failure is other than EPERM, it will terminate.
    * 66log: Redirects Standard Input to the socket of the `66-log` program. This is the default.
    * null: Redirects Standard Input to `/dev/null`
    * parent: This is a no-op redirection. The Standard Input is inherited from the parent process, meaning the [66-supervise](66-supervise.html) program.
    * close: Close the Standard Input.

    Please see [Standard IO redirection](66-standard-io-redirection.html) documentation for further information.

#### StdOut

**Source Snippet**:
```ini
StdOut = 66log
```

Controls standard I/O redirection for the standard output entries.

* mandatory: no

* syntax: [inline](#inline),[simple-colon](#simple-colon)

* valid values:

    * tty:/path/to/tty: Redirects Standard Output to the given tty specified by the path. The path must be absolute and exist. It does not try to take control of the terminal.
    * file:/path/to/file: Redirects Standard Output to the given file specified by the path. The path must be absolute. If the directory of the file and the file itself do not exist, *66* will create it. In that case, the directory will get `0755` permissions and the file will be set with `0666` permissions.
    * console: Redirects Standard Output to the active console. It does not try to take control of the console.
    * 66log: Redirects Standard Output to the socket of the `66-log` program. This is the default.
    * syslog: Redirects Standard Output to the `/dev/log` socket.
    * null: Redirects Standard Output to `/dev/null`.
    * parent: This is a no-op redirection. The Standard Output is inherited from the parent process, meaning the `66-supervise` program.
    * close: Closes the Standard Output.

    Please see [Standard IO redirection](66-standard-io-redirection.html) documentation for further information.

#### StdErr

**Source Snippet**:
```ini
StdErr = inherit
```

Controls standard I/O redirection for the standard error entries.

* mandatory: no

* syntax: [inline](#inline),[simple-colon](#simple-colon)

* valid values:

    * tty:/path/to/tty: Redirects Standard Error to the given tty specified by the path. The path must be absolute and exist. It does not try to take control of the terminal.
    * file:/path/to/file: Redirects Standard Error to the given file specified by path. Path must be absolute. If the directory of file and the file itself doesn't exist, *66* create it. In that case, the directory get `0755` as permissions and the file is set with `0666` as permissions.
    * console: Redirects Standard Error to the active console. It does not try to take control of the console.
    * syslog: Redirects Standard Error to the `/dev/log` socket.
    * null: Redirects Standard Error to `/dev/null`.
    * parent: This is a no-op redirection. The Standard Error is inherited from the parent process, meaning the `66-supervise` program.
    * inherit: Duplicates the Standard Error to the Standard Output. This is the default.
    * close: Closes the Standard Error.

    Please see [Standard IO redirection](66-standard-io-redirection.html) documentation for further information.

#### Provide

**Source Snippet**:
```ini
Provide = ( network networking )
```

Defines one or more service aliases—alternate names under which this service can be referenced. These aliases behave like symbolic links, allowing the same service to be managed with different name.

* mandatory: no

* syntax: [brackets](#brackets)

* valid values:

    * Any arbitrary name.

#### Conflict

**Source Snippet**:
```ini
Conflict = ( connmand networkmanager )
```

Defines one or more services that cannot run or be enabled simultaneously with this service. If a conflicting service is running, attempts to start this service will fail. Similarly, if a conflicting service is enabled, attempts to enable this service will be rejected.

* mandatory: no

* syntax: [brackets](#brackets)

* valid values:

    * Any valid service name.

The following keys are valid **only** when [`Type`](#type) is `event` — they configure an event **source**. See [66-event](66-event.html) for what each family does at runtime.

#### EventType

**Source Snippet**:
```ini
EventType = inotify
```

Selects the family of a `Type = event` source. It is distinct from [`Type`](#type); on a **reactor** the same key lives in the [`[Event]`](#section-event) section instead.

* mandatory: yes for a `Type = event` source; not valid otherwise.

* syntax: [inline](#inline)

* valid values:

    * inotify : watch a filesystem path — pair with [`Watch`](#watch) and [`On`](#on).
    * schedule : fire on a cron/calendar [`Expression`](#expression), optionally in a [`Timezone`](#timezone).
    * timer : fire on a relative interval [`Every`](#every).

#### Watch

**Source Snippet**:
```ini
Watch = /etc/resolv.conf
```

The filesystem path an `inotify` source watches, paired with [`On`](#on).

* mandatory: yes for an `inotify` source; not valid otherwise.

* syntax: [inline](#inline)

* valid values:

    * Any absolute path to an existing file or directory.

* notes:

    `IN_CREATE`/`IN_DELETE`/`IN_MOVED_*` only fire for entries **inside** a watched directory, not for a watched file. A tool that replaces a file atomically (write-temp then rename — dhcpcd, certbot, most editors) does **not** raise `IN_MODIFY` on it; watch the directory (`IN_CREATE`/`IN_MOVED_TO`) or the file itself with `IN_MOVE_SELF`/`IN_DELETE_SELF`.

#### On

**Source Snippet**:
```ini
On = ( IN_CLOSE_WRITE IN_MOVE_SELF )
```

The [inotify(7)](https://man7.org/linux/man-pages/man7/inotify.7.html) event(s) an `inotify` source reacts to on its [`Watch`](#watch) path. (The reactor key [`On`](#on--onall) in the [`[Event]`](#section-event) section is a different vocabulary.)

* mandatory: yes for an `inotify` source; not valid otherwise.

* syntax: [brackets](#brackets) — parentheses required, even for a single value.

* valid values:

    * One or more kernel `inotify` constants: `IN_ACCESS`, `IN_MODIFY`, `IN_ATTRIB`, `IN_CLOSE_WRITE`, `IN_CLOSE_NOWRITE`, `IN_OPEN`, `IN_MOVED_FROM`, `IN_MOVED_TO`, `IN_CREATE`, `IN_DELETE`, `IN_DELETE_SELF`, `IN_MOVE_SELF`, plus the shorthands `IN_MOVE` (`IN_MOVED_FROM`+`IN_MOVED_TO`), `IN_CLOSE` (`IN_CLOSE_WRITE`+`IN_CLOSE_NOWRITE`) and `IN_ALL_EVENTS`. They map straight to the watch mask.

#### Expression

**Source Snippet**:
```ini
Expression = "0 0 3 * * ?"
```

The cron expression of a `schedule` source. The engine is a **Quartz-style** scheduler — **not** classic 5-field Vixie cron.

* mandatory: yes for a `schedule` source; not valid otherwise.

* syntax: [quotes](#quotes)

* valid values — a cron expression of **5, 6 or 7 space-separated fields**:

    ````
    [seconds] minutes hours day-of-month month day-of-week [year]
    ````

    | Fields | Layout |
    |---|---|
    | 5 | `min hour dom month dow` (seconds default to `0`) |
    | 6 | `sec min hour dom month dow` |
    | 7 | `sec min hour dom month dow year` |

    Ranges `0-59`/`0-59`/`0-23`/`1-31`/`1-12`/`0-7`/`1970-2200`. Months accept `JAN`..`DEC`, days accept `SUN`..`SAT` (case-insensitive, `0` = Sunday). Operators: `*` `,` `-` `/` plus Quartz `?` (no specific value), `L` (last), `L-<n>`, `LW` (last weekday), `<n>W` (nearest weekday), `<n>L` (last weekday-n), `<n>#<m>` (m-th weekday-n, `6#3` = 3rd Friday). Macros: `@yearly`/`@annually`, `@monthly`, `@weekly`, `@daily`/`@midnight`, `@hourly`, `@minutely`, `@secondly`.

* notes:

    * You **must** put `?` on either day-of-month or day-of-week — they cannot both carry a value. Even in 5 fields, `"0 3 * * *"` is **rejected**; write `"0 3 * * ?"`.
    * There is **no `@reboot`** macro.
    * The expression is validated at [66 parse](66-parse.html) time; an invalid one fails with a clear error rather than silently at runtime.

#### Timezone

**Source Snippet**:
```ini
Timezone = Europe/Paris
```

The timezone the [`Expression`](#expression) of a `schedule` source is evaluated in.

* mandatory: no; valid only for a `schedule` source.

* syntax: [inline](#inline)

* valid values:

    * Any IANA timezone name (`UTC`, `Europe/Paris`, …), up to 255 characters. When omitted, the schedule is evaluated in **UTC**.

#### Every

**Source Snippet**:
```ini
Every = 30s
```

The period of a `timer` source: a relative, monotonic interval that fires again and again, unaffected by wall-clock changes.

* mandatory: yes for a `timer` source; not valid otherwise.

* syntax: [inline](#inline)

* valid values:

    * A positive whole number with an optional unit suffix — `s` (or none) for seconds, `m` minutes, `h` hours, `d` days. Examples: `30s`, `5m`, `1h`, `90`. The value must be at least one second.

### Section [Start]

This section is *mandatory*. (!)

#### Build (deprecated)

The build type is no longer set by this key — it is detected automatically from the [`Execute`](#execute) field. An `Execute` whose first non-blank line is a shebang (`#!…`) is treated as a **custom** script and run verbatim in that interpreter; otherwise it is an [execline](https://skarnet.org/software/execline) script. See [A word about the Execute key](#a-word-about-the-execute-key).

`Build` is still accepted for now but **ignored**: declaring it only emits a deprecation warning at parse time. Remove it from your frontends.

#### RunAs

**Source Snippet**:
```ini
RunAs = oblive
```
Drops privileges to the specified user or UID:GID before executing the service.

* mandatory: no

* syntax: [inline](#inline),[simple-colon](#simple-colon)

* valid value:

    * Any valid user name set on the system or valid uid:gid number.

        ````
        RunAs = oblive

        RunAs = 1000:19

        # if uid is not specified,
        # the uid of the owner of the process
        # is pick by default
        RunAs = :19

        # if gid is not specified,
        # the gid of the owner of the process
        # is pick by default
        RunAs = 1000:
        ````

        This will pass the privileges of the service to the given user before starting the run script of the service.

    **Note**: (!) The service needs to be first started with root if you want to hand over privileges to a user. Only root can pass on privileges. This field has no effect for other use cases.

#### Execute

**Source Snippet**:
```ini
Execute = ( /usr/bin/auditd -f )
```
Defines the command(s) executed to start the service. Enclose multiple lines in brackets.

* mandatory: yes (!)

* syntax: [brackets](#brackets)

* valid value:

    * The command to execute when starting the service.

    **Note**: The field will be used as is. No changes will be applied at all except in `custom` case(see [A word about the execute key](#a-word-about-the-execute-key)). It's the responsibility of the author to make sure that the content of this field is correct.

#### TimeoutStart

**Source Snippet**:
```ini
TimeoutStart = 2000
```
Specifies the maximum time (in milliseconds) the service may take to **start**. If
the start transition does not complete within this time, `66` kills the service
and reports the transition as failed.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * Any valid number, in milliseconds. The default is `0`, which means no start
      timeout — the service may take as long as it needs to come up.

### Section [Stop]

This section is *optional*.

It handles the stop process of the service. It shares the `RunAs` and `Execute`
keys with [[Start]](#section-start), plus its own `TimeoutStop` key.

#### TimeoutStop

**Source Snippet**:
```ini
TimeoutStop = 5000
```

Specifies the maximum time (in milliseconds) the service's **stop** sequence may
take. If the stop transition — the stop/`finish` script — does not complete within
this time, `66` kills the service.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * Any valid number, in milliseconds. The default is `0`, which means no stop
      timeout — the stop script may run as long as it needs.

### Section [Logger]

This section is optional and controls the behavior of the default logging system used by *66*, which is handled by its native `66-log` program.

It will only have effects if value *log* was **not** prefixed by an exclamation mark to the [`Options`](#options) key in the [[Main]](#section-main) section. Additionally, the `StdIn` or `StdOut` keys from the [[Main]](#section-main) **must be set** to `66log`, or these keys **must not** be defined at all.

This section extends the `RunAs` and `Execute` key fields from [[Start]](#section-start) and the `TimeoutStart` and `TimeoutStop` key fields from [[Start]](#section-start) and [[Stop]](#section-stop) . These are also valid keys for [[Logger]](#section-logger) and behave the same way they do in the other sections, but none of them is mandatory—see example below. When they are not specified, the default behaviour for those keys is applied.

Furthermore there are some keys specific to the log.

The following key names are also valid:

- `RunAs` and `Execute` — See [[Start]](#section-start)
- `TimeoutStart` — See [[Start]](#section-start); `TimeoutStop` — See [[Stop]](#section-stop)

#### Backup

**Source Snippet**:
```ini
Backup = 3
```
Number of rotated log files to retain before overwriting the oldest.

* mandatory: no

* syntax: [uint](#uint)

* valid value:

    * Any valid number.

        The log directory will keep *value* files. The next log to be saved will replace the oldest file present. By default `3` files are kept.

#### MaxSize

**Source Snippet**:
```ini
MaxSize = 1000000
```
Byte threshold to trigger log rotation when the current file grows too large.

* mandatory: no

* syntax: [uint](#uint)

* valid value:

    * Any valid number.

        A new log file will be created every time the current one approaches *value* bytes. By default, filesize is `1000000`; it cannot be set lower than `4096` or higher than `268435455`.

#### Timestamp

**Source Snippet**:
```ini
Timestamp = iso
```

Specifies timestamp format prefixed to each log entry. If not specified, it defaults to `iso` (configurable at compile time).

* mandatory: no

* syntax: [inline](#inline)

* valid value:

    * tai

        The logged line will be preceded by a TAI64N timestamp (and a space) before being processed by the next action directive.

    * iso

        The selected line will be preceded by a ISO 8601 timestamp for combined date and time representing local time according to the systems timezone, with a space (not a `T`) between the date and the time and two spaces after the time, before being processed by the next action directive.

    * none

        The logged line will not be preceded by any timestamp.

        The following are two possible examples for the [[Logger]](#section-logger) section definition.

        ````
        [Logger]
        RunAs = user
        TimeoutStop = 10000
        Backup = 10
        Timestamp = iso
        ````
        ````
        [Logger]
        Backup = 10
        ````

### Section [Environment]

This section is *optional*.

A file containing the `key=value` pair(s) will be created by default at `%%service_admconf%%/name_of_service` directory. The default can also be changed at compile-time by passing the `-D sysadmin-service-conf-dir=DIR` option to `meson setup`.

#### Any `key=value` pair

**Source Snippet**:
```ini
DirRun=/run/openntpd
```

* mandatory: no

* syntax: [pair](#pair)

* valid value:

    * You can define any variables that you want to add to the environment of the service. For example:

        ````
        [Environment]
        dir_run=/run/openntpd
        cmd_args=-d -s
        ````

        The `!` character can precede the value. Ensure **no** space exists between the exclamation mark and the *value*. This action explicitly avoids setting the value of the *key* for the runtime process but only applies it at the start of the service. For instance, the following valid example unsets the `key=value` pair `dir_run=!/run/openntpd` from the general environment variables of the service.

        the following syntax is valid

        ````
        [Environment]
        dir_run=!/run/openntpd
        cmd_args = !-d -s
        ````
        where this one is not

        ````
        [Environment]
        dir_run=! /run/openntpd
        cmd_args = ! -d -s
        ````

        Refers to [execl-envfile](execl-envfile.html) for further information.

#### ImportFile

**Source Snippet**:
```ini
ImportFile=/etc/66/init.conf
```

The `ImportFile` variable is recognized by `66` and treated as a `key=value` pair, similar to other environment variables. However, `ImportFile` itself is not exported to the environment.

The target file must adhere to the environment definition syntax specified in the [file syntax](execl-envfile.html#file-syntax) guidelines.

* mandatory: no

* syntax: [path](#path)

* valid value:

    * Any valid absolute file path can be specified. The `ImportFile` variable can be defined multiple times. For example:

        ````
        [Environment]
        dir_run=/run/openntpd
        ImportFile=/etc/66/init.conf
        ````

        The `!` character has no effect on `ImportFile`.

        `ImportFile` processing occurs at the end of the environment setup. If a key is defined both in the `[Environment]` section and in a file specified by `ImportFile`, the value from the `ImportFile` takes precedence.

        For multiple `ImportFile` declarations, the last declared file takes precedence for any duplicate keys found across the specified files.

        [identifier](66-identifier.html) is still also **interpreted**. For example:

        ````
        [Environment]
        dir_run=/run/openntpd
        ImportFile=/etc/66/init.conf
        ImportFile=@H/.66/environment/my.conf
        ````

### Section [Regex]

This section is *optional*.

It will only have an effect when the service is a `module` type—see the section [Module service creation](66-module-creation.html).

[identifier](66-identifier.html) are replaced before applying the regex section.

#### Configure

**Source Snippet**:
```ini
Configure = "--enable-feature"
```

Arguments passed to the module’s `configure` script.

* mandatory: no

* syntax: [quotes](#quotes)

* valid value:

    * You can define any arguments to pass to the module's configure script.

#### Directories

**Source Snippet**:
```ini
Directories = ( DM=sddm )
```

Regex-based renaming rules for module subdirectories. Each entry is `regex=replacement`.

* mandatory: no

* syntax: [pair](#pair) inside [brackets](#brackets)

* valid value:

    * Any `key=value` pair where key is the regex to search on the directory name and value the replacement of that regex. For example:

        ````
        Directories = ( DM=sddm TRACKER=consolekit )
        ````

        Where the module directory contains two sub-directories named use-DM and by-TRACKER directories. It will be renamed as use-sddm and by-consolekit respectively.

#### Files

**Source Snippet**:
```ini
Files = ( servicename=newname )
```

Regex-based renaming rules for files. Each entry is `regex=replacement`.

* mandatory: no

* syntax: [pair](#pair) inside [brackets](#brackets)

* valid value:

    * Reacts exactly as Directories field but on files name instead of directories name.

#### InFiles

**Source Snippet**:
```ini
InFiles = ( :mount-tmp:args=-o noexec )
```

In-file regex replacements for module files. Use `:filename:regex=replacement` or `::regex=replacement` for all files.

* mandatory: no

* syntax: [colon](#colon) inside [brackets](#brackets)

* valid value:

    * Any valid filename between the double colon with any `key=value` pair where key is the regex to search inside the file and value the replacement of that regex. The double colon **must** be present but the name between it can be omitted. In that case, the `key=value` pair will apply to all files contained on the module directories and to all keys (regex) found inside the same file.For example:

        ````
        InFiles = ( :mount-tmp:args=-o noexec
        ::user=@I )
        ````

        * It replaces first the term `@I` by the name of the module.
        * It opens the file named mount-tmp, search for the args regex and replaces it by the value of the regex.
        * It opens all files found on the module directory and replaces all regex 'user' found by the name of the module in each file.

### Section [Execute]

This section is *optional*.

This section configures tasks executed just before calling `exec` for the service’s start and stop processes.

Resource limits (e.g., `LimitNICE`, `LimitAS`) sets soft (rlim_cur) and hard (rlim_max) limits by retrieving current limits with `getrlimit()`, adjusting the hard limit for root-owned services if needed, capping the soft limit to the hard limit for non-root services, and applying the new limits with `setrlimit()`. If a limit is zero, no changes are made.

All LimitXXX keys accept the value `unlimited` to set the corresponding `RLIMIT_*` resource to `RLIM_INFINITY`, allowing unrestricted use of that resource. For unprivileged services (non-root users), `unlimited` or values exceeding the current hard limit (`rlim_max`) are capped at `rlim_max` to prevent errors. Setting `unlimited` or raising limits beyond `rlim_max` requires root privileges or `CAP_SYS_RESOURCE`. Linux-specific limits (e.g., LimitNICE, LimitRTPRIO) are ignored on systems where they are not supported.

#### LimitAS

**Source Snippet**:
```ini
LimitAS = unlimited
```

Specifies the maximum address space (virtual memory) for the service process, in bytes. Corresponds to the `RLIMIT_AS` resource limit.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 1048576 for 1 MB).

#### LimitCORE

**Source Snippet**:
```ini
LimitCORE = 0
```

Specifies the maximum size of core dump files generated by the service process, in bytes. Corresponds to the `RLIMIT_CORE` resource limit.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 0 to disable core dumps, 1048576 for 1 MB).

* notes:

    Setting to 0 disables core dumps.

#### LimitCPU

**Source Snippet**:
```ini
LimitCPU = unlimited
```

Specifies the maximum CPU time the service process can use, in seconds. Corresponds to the `RLIMIT_CPU` resource limit.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 3600 for 1 hour).

* notes:

    Exceeding the limit sends SIGXCPU to the process.

#### LimitDATA

**Source Snippet**:
```ini
LimitDATA = 5242880
```

Specifies the maximum size of the service process’s data segment, in bytes. Corresponds to the `RLIMIT_DATA` resource limit.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 5242880 for 5 MB).

* notes:

    Affects memory allocations (e.g., malloc()).

#### LimitFSIZE

**Source Snippet**:
```ini
LimitFSIZE = unlimited
```

Specifies the maximum size of files the service process can create, in bytes. Corresponds to the `RLIMIT_FSIZE` resource limit.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 1048576 for 1 MB).

* notes:

    Exceeding the limit sends SIGXFSZ to the process.

#### LimitLOCKS

**Source Snippet**:
```ini
LimitLOCKS = 1024
```

Specifies the maximum number of file locks the service process can hold. Corresponds to the `RLIMIT_LOCKS` resource limit (Linux-specific).

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 1024).

* notes:

    Only available on Linux; ignored on systems lacking RLIMIT_LOCKS.

#### LimitMEMLOCK

**Source Snippet**:
```ini
LimitMEMLOCK = 65536
```

Specifies the maximum amount of memory the service process can lock into RAM, in bytes. Corresponds to the `RLIMIT_MEMLOCK` resource limit.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 65536 for 64 KB).

* notes:

    Affects mlock() and similar calls.

#### LimitMSGQUEUE

**Source Snippet**:
```ini
LimitMSGQUEUE = 819200
```

Specifies the maximum size of POSIX message queues the service process can create, in bytes. Corresponds to the `RLIMIT_MSGQUEUE` resource limit (Linux-specific).

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 819200).

* notes:

    Only available on Linux; ignored on systems lacking `RLIMIT_MSGQUEUE`.

#### LimitNICE

**Source Snippet**:
```ini
LimitNICE = -20
```

Specifies the maximum nice value (scheduling priority) the service process can set. Corresponds to the `RLIMIT_NICE` resource limit (Linux-specific).

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * An integer between `-20` and `19` (e.g., `-20` for highest priority, `19` for lowest).

* notes:

    Lower values (e.g., `0`) give higher CPU priority; higher values (e.g., `19`) give lower priority.
    Only available on Linux; ignored on systems lacking RLIMIT_NICE.

    Numeric values are adjusted to the hard limit if exceeded (e.g., `-20` may be capped to 0 if `ulimit -He` is `20`).

    Setting negative nice values may require `CAP_SYS_NICE` for unprivileged processes.

#### LimitNOFILE

**Source Snippet**:
```ini
LimitNOFILE = 1024
```

Specifies the maximum number of open file descriptors the service process can have. Corresponds to the `RLIMIT_NOFILE` resource limit.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 1024).

* notes:

    Affects files, sockets, and pipes.

#### LimitNPROC

**Source Snippet**:
```ini
LimitNPROC = 4096
```

Specifies the maximum number of processes the service process’s user can create. Corresponds to the `RLIMIT_NPROC` resource limit.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 4096).

* notes:

    Applies to the user’s total processes, not just the service.

#### LimitRTPRIO

**Source Snippet**:
```ini
LimitRTPRIO = 50
```

Specifies the maximum real-time priority the service process can set. Corresponds to the `RLIMIT_RTPRIO` resource limit (Linux-specific).

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * An integer between 0 and 100 (e.g., 50; 0 disables real-time priority).

* notes:

    Higher values give higher real-time scheduling priority.
    Only available on Linux; ignored on systems lacking RLIMIT_RTPRIO.

#### LimitRTTIME

**Source Snippet**:
```ini
LimitRTTIME = unlimited
```

Specifies the maximum real-time CPU time the service process can use, in microseconds. Corresponds to the `RLIMIT_RTTIME` resource limit (Linux-specific).

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 1000000 for 1 second).

* notes:

    Only available on Linux; ignored on systems lacking RLIMIT_RTTIME.

#### LimitSIGPENDING

**Source Snippet**:
```ini
LimitSIGPENDING = 8192
```

Specifies the maximum number of queued signals the service process can have. Corresponds to the `RLIMIT_SIGPENDING` resource limit (Linux-specific).

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 8192).

* notes:

    Only available on Linux; ignored on systems lacking `RLIMIT_SIGPENDING`.

#### LimitSTACK

**Source Snippet**:
```ini
LimitSTACK = 8388608
```

Specifies the maximum stack size for the service process, in bytes. Corresponds to the RLIMIT_STACK resource limit.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * A non-negative integer (e.g., 8388608 for 8 MB).

* notes:

    Affects thread stacks and recursion depth.

#### BlockPrivileges

**Source Snippet**:
```ini
BlockPrivileges = true
```

Enables the Linux `PR_SET_NO_NEW_PRIVS` flag via `prctl()`, preventing the service process and its children from gaining additional privileges (e.g., via `setuid` binaries or capability inheritance).

* mandatory: no

* syntax: [boolean](#boolean)

* valid values:

    * A boolean value

* notes:

    Once set, cannot be unset for the process or its children.

#### UMask

**Source Snippet**:
```ini
UMask = 022
```

Sets the file creation mask for the service process via `umask()`, controlling default permissions for newly created files and directories. The value is specified in octal notation, determining which permission bits are masked from the default mode.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * An octal number between `000` and `777` (e.g., `022`, `002`, `077`).

    * Undefined: Defaults to system-wide configuration.

#### Nice

**Source Snippet**:
```ini
Nice = -10
```

Sets the CPU scheduling priority (nice value) for the service process via `setpriority()`, affecting how the kernel allocates CPU time. Lower values increase priority; higher values decrease it.

* mandatory: no

* syntax: [uint](#uint)

* valid values:

    * An integer between `-20` (highest priority) and `19` (lowest priority).

    * Undefined: Defaults to system-wide configuration.

* notes:

    Negative values (e.g., `-10`) require `CAP_SYS_NICE` for unprivileged services (non-root users) or root privileges.

    Must be within the `RLIMIT_NICE` limit set by `LimitNICE`.

    Affects the service process and its children.

#### ChangeDirectory

**Source Snippet**:
```ini
ChangeDirectory = /var/lib/myservice
```

Sets the working directory for the service process via `chdir()`, affecting the default directory for file operations (e.g., opening files with relative paths).

* mandatory: no

* syntax: [path](#path)

* valid values:

    * Any valid absolute file path can be specified.

    * Undefined: Inherits the working directory from the parent process (default, typically the supervision directory).

* notes:

    The directory must exist and be accessible (readable and executable) by the service’s user. Permission or non-existent directory errors cause the service to fail with a logged warning.

    Affects the service process and its children.

#### CapsBound

**Source Snippet**:
```ini
CapsBound = (CAP_SYS_NICE CAP_CHOWN)
```

Defines the Linux capabilities allowed in the capability bounding set for a root-owned service’s process. This setting controls which special permissions (like adjusting process priorities or changing file ownership) the service can use, restricting it to only the listed capabilities or excluding specific ones.

* mandatory: no

* syntax: [brackets](#brackets)

* valid values:

    * A space-separated list of capability names in parentheses, such as (`CAP_SYS_NICE` `CAP_CHOWN` `CAP_DAC_OVERRIDE`).

    * Capability names can be prefixed with `!` to exclude them, allowing all other capabilities. For example, (`CAP_NET_ADMIN` `!CAP_SYS_ADMIN` `CAP_MAC_OVERRIDE` `!CAP_SYS_RESOURCE`) allows all capabilities except `CAP_SYS_ADMIN` and `CAP_SYS_RESOURCE`.

    * Valid capability names include `CAP_SYS_NICE`, `CAP_CHOWN`, `CAP_DAC_OVERRIDE`, `CAP_SYS_ADMIN`, and others (see Linux documentation for the full list).

    * Undefined: No changes are made to the bounding set, and the service uses the system’s default permissions.

* notes:

    Only applies to services running as the root user. For non-root services, this setting is silently ignored.

    Clears all existing capabilities in the bounding set before applying the listed ones, ensuring only specified capabilities are allowed.

    If any capability name is prefixed with `!`, the list is interpreted as allowing all capabilities except those marked with `!`. For example, (`CAP_NET_ADMIN` `!CAP_SYS_ADMIN`) allows all capabilities except `CAP_SYS_ADMIN`. Where doing, (`CAP_SYS_NICE` `CAP_CHOWN`) restricts the bounding set to only `CAP_SYS_NICE` and `CAP_CHOWN`.

    Invalid capability names are ignored, and a warning is logged when the service configuration is parsed.

    To allow privileges to be dropped, it is necessary to set `CAP_SETUID` and `CAP_SETGID` to the capability bounding set if you use the `RunAs` key.

    Requires Linux kernel version `5.6` or later.

#### CapsAmbient

**Source Snippet**:
```ini
CapsAmbient = (CAP_SYS_NICE)
```

Specifies Linux capabilities that a service and its child processes automatically retain, even when starting new programs. This allows permissions, such as adjusting process priorities, to be passed to child processes without requiring root privileges.

* mandatory: no

* syntax: [brackets](#brackets)

* valid values:

    * A space-separated list of capability names in parentheses, such as `(CAP_SYS_NICE CAP_CHOWN CAP_DAC_OVERRIDE)`.

    * Capability names can be prefixed with `!` to exclude them, allowing all other capabilities. For example, `(CAP_SYS_NICE !CAP_SYS_ADMIN)` includes all capabilities except `CAP_SYS_ADMIN`.

    * Undefined: No ambient capabilities are set, and child processes inherit no special permissions.

* notes:

    Applies to both root-owned services and non-root services.

    The bounding set must contain at least `CAP_SETPCAP` capability and each listed capability. If not, it is skipped, and a warning is logged. If `CAP_SETPCAP` is not in bounding set, the process dies.

    For root-owned services, if `CapsBound` is not set, the service checks the system’s current set of allowed permissions to decide which capabilities can be used. If `CapsBound` is set, only the capabilities listed in `CapsBound` are considered. For example, if `CapsBound = (CAP_SYS_NICE)` and `CapsAmbient = (CAP_DAC_OVERRIDE)`, the `CAP_DAC_OVERRIDE` capability will be skipped because it is not in the `CapsBound` list, and a warning will be logged.

    Requires Linux kernel version `5.6` or later.

### Section [Event]

This section is *optional*. It turns an ordinary `classic`, `oneshot` or `module` service into an event **reactor**: when its trigger fires, the service runs a `66` command **on itself** ([`Do`](#do)) and/or raises a named event ([`Emit`](#emit)). A frontend carries **at most one** rule — a service is either a reactor *or* a `Type = event` source, never both. See [66-event](66-event.html) for the full model and the runtime behaviour, and [66-eventd](66-eventd.html) for the daemon that runs the rules.

#### EventType

**Source Snippet**:
```ini
EventType = service
```

Selects which family of trigger the reactor subscribes to, and therefore which [`On`](#on--onall) vocabulary applies. Same key, same values as the source [`EventType`](#eventtype) in `[Main]`, but placed here for a reactor.

* mandatory: yes for a reactor.

* syntax: [inline](#inline)

* valid values: `service`, `signal`, `user`, `inotify`, `schedule`, `timer`.

    * service : react to the status transitions (up/down/crash/…) of a supervised service named in [`From`](#from).
    * signal : react to a signal routed by `66` to a supervised service named in [`From`](#from).
    * user : react to a name raised by [66 emit](66-emit.html) or by another reactor's [`Emit`](#emit). A `user` reactor is **sourceless** — no [`From`](#from).
    * inotify / schedule / timer : react to the `Type = event` source named in [`From`](#from). The condition lives in the source, so these carry **no** [`On`](#on--onall).

#### From

**Source Snippet**:
```ini
From = ( rabbitmq )
```

The source(s) the reactor subscribes to. **Always explicit** — sources are never inferred from [`On`](#on--onall).

* mandatory: yes for every reactor **except** `user` (which is sourceless).

* syntax: [brackets](#brackets)

* valid values:

    * The name of a service. For `service`/`signal` it is a supervised service; for `inotify`/`schedule`/`timer` it is the name of the `Type = event` source.

* notes:

    Each `From` source also becomes a **dependency** of the reactor, so `66` starts (or arms) the source before it arms the reactor. Consequently `66-eventd` reads the source's current state at arm time: a reactor whose source is already in the awaited state **fires immediately**, instead of waiting for the next transition. Likewise, `66 free <source>` disarms the reactors that depend on it.

#### On / OnAll

**Source Snippet**:
```ini
On    = ( down )
OnAll = ( auth:up db:up )
```

The trigger condition(s) of a `service`, `signal` or `user` reactor. Use **exactly one** of the two keys:

* `On` — a single condition, or a bracketed list treated as **OR** (fires if any listed condition matches).
* `OnAll` — an **AND** over the *current* states: fires only when all listed conditions hold at once. Valid only for `service` and `signal` reactors; a `user` reactor (whose conditions are momentary names, not states) uses `On` only.

* mandatory: yes for `service`, `signal` and `user` reactors; **forbidden** for `inotify`/`schedule`/`timer` reactors (the condition is the source's own [`On`](#on)).

* syntax: [brackets](#brackets) — parentheses required, even for a single value.

* valid values — depend on [`EventType`](#eventtype-1):

    * service : a status **state** word — `down`, `starting`, `up`, `stopping`, `finishing`, `restarting`, `done`, `failed` — or a status **result** word — `success`, `exited`, `signaled`, `timeout-start`, `timeout-stop`, `crash-limit`, `exec-failed`. Two results take an argument: `exited:<code>` and `signaled:<SIG>` (e.g. `signaled:SIGKILL`). These are exactly the words [66 status](66-status.html) prints. (`signaled` means *the process died from a signal*, unlike a `signal` reactor which means *a routed signal was received*.)
    * signal : a signal name, e.g. `SIGHUP`.
    * user : the emitted name, e.g. `backend-down`.

* per-source form: in a list, a bare token (`up`) applies to **all** sources of [`From`](#from); a `service:condition` token (`auth:up`) scopes the condition to one named source, which must be a member of `From`.

#### Do

**Source Snippet**:
```ini
Do = restart
```

The `66` command the reactor runs **on itself** when the trigger fires.

* mandatory: no on its own, but a reactor must define at least one of `Do` / [`Emit`](#emit).

* syntax: [inline](#inline)

* valid values: exactly one of `start`, `stop`, `restart`, `reload`, `reconfigure`, `free` — the matching `66` command. Bare command only, no argument.

* notes:

    The command is gated by the reactor's current state (see [66-event](66-event.html#runtime-behaviour)): `Do = start` is a no-op on an already-up service; `stop`/`restart`/`reload` act only on an up service; `reconfigure`/`free` always act.

#### Emit

**Source Snippet**:
```ini
Emit = backend-down
```

Raises a `user` event of the given name when the trigger fires, **independently** of [`Do`](#do). This is how reactions chain: another reactor with `EventType = user` and `On = ( <name> )` fires in turn (as would `66 emit <name>`). A reactor may carry `Do`, `Emit`, or both.

* mandatory: no on its own, but a reactor must define at least one of [`Do`](#do) / `Emit`.

* syntax: [inline](#inline)

* valid values: any name. It matches the [`On`](#on--onall) of a `user` reactor.

## A word about the Execute key

The `Execute` key can be written in any language. Make the **first non-blank line** of the field a shebang (`#!/usr/bin/bash`, `#!/usr/bin/python3`, …): `66` detects it and treats the script as **custom**, running it verbatim in that interpreter. Without a shebang, the field is an [execline](https://skarnet.org/software/execline) script. For example, to write your `Execute` field with bash:

```
Execute = (#!/usr/bin/bash
echo "This script displays available services"
for i in $(ls %%service_system%%); do
    echo "daemon : ${i} is available"
done
)
```

This is an unnecessary example but it shows how to construct this use case. The resulting file will be :

```
#!/usr/bin/bash
echo "This script displays available services"
for i in $(ls %%service_system%%); do
    echo "daemon : ${i} is available"
done
```

The parser duplicates exactly what appears between `(` and `)`, preserving all characters as they are. However, it removes any carriage return (`\r`), tab (`\t`), space, or newline (`\n`) located between the opening parenthesis and the `#` of the shebang declaration. No other characters are permitted in this span. For instance, if you write

```
Execute = (

    #!/bin/bash
    echo hello world!
)
```

the final result will be

```
#!/bin/bash
    echo hello world!
```

ensuring that the very first line of the script is the declaration of the shebang to avoid an ***Exec format error***.

Note that in a custom (shebang) script, variables will **not be replaced** by their corresponding environment values within the script, unlike the behavior with the execlineb script format.

[identifier](66-identifier.html) is still also **interpreted** even in custom script.

This same behavior applies to the [[Logger]](#section-logger) section. Also, The fields `Backup`, `MaxSize` and `Timestamp` will have **no effect** in a custom case. You need to explicitly define the program to use the logger and the options for it in your `Execute` field.

## A word about the Version key

The `Version` key supports formats inspired by semantic versioning (e.g., `"1.0.0"`) but is flexible enough to handle any number of components separated by dots or other non-alphanumeric characters, pre-release tags (e.g., `"1.0.0-alpha"`), mixed components (e.g., `"0ab"`), and complex strings (e.g., `"1.0ab.01-1"`). This following explains what constitutes a valid version string and what does not, helping users effectively utilize the field.

### What Can Be Used as a Version String

The `Version` key accepts version strings composed of components separated by any number of non-alphanumeric characters (e.g., dots, hyphens). Components can be numeric (e.g., `"123"`), alphabetic (e.g., `"alpha"`), or mixed (e.g., `"123abc"`). The function handles leading zeros, pre-release tags, letter suffixes, and any number of components (not limited to three dots). Below are the characteristics of valid version strings:

**Numeric Versions with Any Number of Components**:
   - Strings like `"1"`, `"1.0"`, `"1.0.0"`, `"1.0.0.0"`, or `"10.0.1.2.3"`.
   - The number of dots (or other separators) is not restricted to three; you can have zero, one, two, three, four, or more components (e.g., `"1.0"` or `"1.0.0.0.0"`).
   - Numbers can include leading zeros, which are ignored during comparison (e.g., `"01.00.00"` is equivalent to `"1.0.0"`).
   - Components are separated by any non-alphanumeric characters (e.g., `"1-0-0"`, `"1..0--0"`, `"1.0.0.0_0"`).

**Pre-release Versions**:
   - Versions with alphabetic pre-release tags, such as `"1.0.0-alpha"`, `"2.0.0-beta"`, or `"1.0.0.0-rc1"`.
   - Pre-release tags (e.g., `"alpha"`, `"beta"`) are treated as higher precedence than stable versions (e.g., `"1.0.0-alpha" < "1.0.0"`).
   - Tags are case-insensitive (e.g., `"1.0.0-ALPHA"` is equivalent to `"1.0.0-alpha"`).
   - Only the first letter is taken into account whatever the length of the string.

**Mixed Components**:
   - Components that combine numeric and alphabetic parts without a separator, such as `"123abc"` or `"0ab"`, are valid.
   - These are parsed as a numeric component followed by an alphabetic suffix:
     - `"123abc"` splits into numeric `"123"` and alphabetic `"abc"`.
     - `"0ab"` splits into numeric `"0"` and alphabetic `"ab"`.
   - Example: `"1.0.0-123abc"` is parsed as numeric `"123"` followed by an alphabetic suffix `"abc"`.
   - Only the first letter is taken into account whatever the length of the string.

**Letter Suffixes**:
   - Versions with an alphabetic suffix after a pre-release tag or mixed component, such as `"1.0.0-alpha.1"`, `"1.0.0-beta.patch"`, or `"1.0ab.01-1"`.
   - The alphabetic part is treated as a separate component with lower precedence than numeric components.

**Complex Version Strings**:
   - Strings combining multiple component types with any number of separators, such as `"1.0ab.01-1"` or `"1.0.0.0.0-alpha.2"`, are valid.
   - Example breakdown of `"1.0ab.01-1"`:
     - `"1"`: Numeric component.
     - `"0ab"`: Numeric `"0"` + alphabetic suffix `"ab"`.
     - `"01"`: Numeric component (equivalent to `"1"`).
     - `"1"`: Numeric component.

**Shortened or Extended Versions**:
   - Versions with any number of components are valid, from a single component (e.g., `"1"`) to many (e.g., `"1.0.0.0.0"`).
   - Shorter versions are treated as equivalent to versions padded with zeros (e.g., `"1.0"` is equivalent to `"1.0.0"`, `"1"` is equivalent to `"1.0.0.0"`).
   - Extended versions with more components are compared component-by-component (e.g., `"1.0.0.0" == "1.0.0"`).

**Empty Strings**:
   - An empty string (`""`) is valid and treated as a version with a single zero component (equivalent to `"0"`).

**Separators**:
   - Any non-alphanumeric character (e.g., `.`, `-`, `_`, `+`) can act as a separator, and any number of consecutive separators is allowed and ignored (e.g., `"1..0"` is equivalent to `"1.0"`, `"1---0..0"` is equivalent to `"1.0.0"`).
   - Separators are flexible, so `"1-0-0"`, `"1_0_0"`, and `"1.0.0"` are equivalent.

**Examples of Valid Version Strings**:
- `"1"`
- `"1.0"`
- `"1.0.0"`
- `"1.0.0.0"`
- `"1.0.0.0.0"`
- `"2.0.0-alpha"`
- `"1.0.0-beta.1"`
- `"01.00.00"`
- `"1-0-0"`
- `"1.0.0-rc.2"`
- `"1.0ab.01-1"`
- `"1.0.0-123abc"`
- `""`
- `"2.0.0--alpha..patch"`
- `"1-0ab-01--1"`
- `"10.0.1.2.3"`

### What Cannot Be Used as a Version String

While the `Version` key is robust, certain inputs are invalid or problematic. Users should avoid the following:

**Special Characters in Components**:
   - Components should consist of numeric (`0-9`) or alphabetic (`a-z`, `A-Z`) characters. Special characters like `@`, `#`, or `$` within components (not as separators) are not supported and may lead to incorrect parsing.
   - Example: `"1.0.0@alpha"` is invalid because `@alpha` contains an unsupported character in the component.

**Whitespace in Components**:
   - Whitespace within components (e.g., `"1.0.0 alpha"`) is treated as a separator, which may split components unexpectedly. Use hyphens or dots for pre-release tags (e.g., `"1.0.0-alpha"`).
   - Example: `"1.0.0 alpha"` is valid from an algorithm point of view but the parsed will only consider the first element.

**Excessively Long Strings**:
   - Extremely long version strings (e.g., thousands of characters or hundreds of components) may cause performance issues or stack overflows due to the fixed-size arrays in the function. Keep version strings reasonably short (e.g., under 50 characters).
   - Example: A string with hundreds of components is technically valid but impractical.

**Examples of Invalid or Problematic Version Strings**:
- `NULL` (causes undefined behavior).
- `"1.0.0@alpha"` (invalid character `@` in component).
- `"1.0.0#patch"` (invalid character `#` in component).
- `"1.0.0 alpha"` (whitespace splits components unexpectedly, likely not intended).
- A 51-character or higher string is invalid.

## Prototype of a frontend file

The minimal template is e.g.:

```
[Main]
Type = classic

[Start]
Execute = ( /usr/bin/true )
```

This prototype contain all valid section with all valid `key=value` pair.

```
[Main]
Type =
Description = ""
Version =
Depends = ()
RequiredBy = ()
OptsDepends = ()
Options = ()
Flags = ()
Notify =
User = ()
MaxDeath =
MaxDeathInterval =
DownSignal =
CopyFrom = ()
InTree =
StdIn =
StdOut =
StdErr =
Provide = ()
Conflict = ()
EventType =
Watch =
On = ()
Expression = ""
Timezone =
Every =

[Start]
RunAs =
Execute = ()
TimeoutStart =

[Stop]
RunAs =
Execute = ()
TimeoutStop =

[Logger]
RunAs =
Backup =
MaxSize =
Timestamp =
TimeoutStart =
TimeoutStop =
Execute = ()

[Environment]
ImportFile=/path/to/file
mykey=myvalue
ANOTHERKEY=!anothervalue

[Regex]
Configure = ""
Directories = ()
Files = ()
InFiles = ()

[Execute]
LimitAS =
LimitCORE =
LimitCPU =
LimitDATA =
LimitFSIZE =
LimitLOCKS =
LimitMEMLOCK =
LimitMSGQUEUE =
LimitNICE =
LimitNOFILE =
LimitNPROC =
LimitRTPRIO =
LimitRTTIME =
LimitSIGPENDING =
LimitSTACK =
BlockPrivileges =
UMask =
ChangeDirectory = /directory/path
CapsBound = ()
CapsAmbient = ()

[Event]
EventType =
From = ()
On = ()
OnAll = ()
Do =
Emit =
```

The `[Main]` event keys (`EventType`, `Watch`, `On`, `Expression`, `Timezone`, `Every`) apply only to a `Type = event` **source**; the `[Event]` section applies only to a **reactor**. A frontend never holds both.
