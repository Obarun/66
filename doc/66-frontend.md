title: The 66 Suite: frontend
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# The frontend service file

The [s6](https://skarnet.org/software/s6) programs use different files. It is quite complex to understand and manage the relationship between all those files. If you're interested in the details you should read [the documentation for the s6 servicedir](https://skarnet.org/software/s6/servicedir.html) and also about [classic](https://skarnet.org/software/s6/servicedir.html) and [module](66-module-creation.html) services. The frontend service file of `66` program allows you to deal with all these different services in a centralized manner and in one single location.

By default `66` program expects to find service files in `%%service_system%%` and `%%service_adm%%` for root user, `%%service_system%%/user` and `%%service_adm/user%%` for regular accounts. For regular accounts, `$HOME/%%service_user%%` will take priority over the previous ones. Although this can be changed at compile time by passing the `--with-system-service=DIR`, `--with-sysadmin-service=DIR` and `--with-user-service=DIRoption` to `./configure`.

The frontend service file has a format of `INI` with a specific syntax on the key field. The name of the file usually corresponds to the name of the daemon and does not have any extension or prefix.

The file is made of *sections* which can contain one or more `key value` pairs where the *key* name can contain special characters like `-` (hyphen) or `_` (low line) except the character `@` (commercial at) which is reserved.

You can find a prototype with all valid section and all valid `key=value` pair at the end of this [document](66-frontend.html#prototype-of-a-frontend-file).

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
    Version = 0.1.0
    User = ( root )

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

The `[Main]` section **must be** declared first.

## Sections

All sections need to be declared with the name written between square brackets `[]` and **must begins** with a uppercase followed by lowercase letters **only**. This means that special characters and numbers are not allowed in the name of a section.

 The frontend service file allows the following section names:

- [[Main]](66-frontend.html#section-main)
- [[Start]](66-frontend.html#section-start)
- [[Stop]](66-frontend.html#section-stop)
- [[Logger]](66-frontend.html#section-logger)
- [[Environment]](66-frontend.html#section-environment)
- [[Regex]](66-frontend.html#section-regex)

Although a section can be mandatory not all of its key fields must be necessarily so.

---

## Syntax legend

The *value* of a *key* is parsed in a specific format depending on the key. The following is a break down of how to write these syntaxes.

---

## *inline*

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

----

## *quotes*

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

----

## *brackets*

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

----

## *uint*
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

----

## *path*

An absolute path beginning with a forward slash `/`. **Must** be on the same line with its corresponding *key*.

* Valid syntax:

    ````
    Destination = /etc/66

    Destination=/etc/66
    ````

* **(!)** Invalid syntax:

    ````
    Destination=/a/very/
    long/path
    ````

----

## *pair*

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

----

## *colon*

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

---

## *simple-colon*

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

----

## Section [Main]

This section is *mandatory*. (!)

### Valid *key* names:

- Type

    Declare the type of the service.

    mandatory : yes (!)

    syntax : [inline](#inline)

    valid values :

    * classic : declares the service as a `classic` service.
    * oneshot : declares the service as a `oneshot` service.
    * module : declares the service as a `module` service.

    ---

- Version

    Version number of the service.

    mandatory : yes (!)

    syntax : [inline](#inline)

    valid values :

    * Any valid version number under the form `digit.digit.digit`.

        For example, the following is valid:

        ````
            Version = 0.1.0
        ````

        where:

        ````
            Version = 0.1.0.1
            Version = 0.1
            Version = 0.1.rc1
        ````

        is not.

    ---

- Description

    A short description of the service.

    mandatory : yes (!)

    syntax : quote

    valid values :

    * Anything you want.

    ---

- User

    Declare the permissions of the service.

    mandatory : yes (!)

    syntax : [bracket](#bracket)

    valid values :

    * Any valid user of the system. If you don't know in advance the name of the user who will deal with the service, you can use the term `user`. In that case every user of the system will be able to deal with the service.

    (!) Be aware that `root` is not automatically added. If you don't declare `root` in this field, you will not be able to use the service even with `root` privileges.

    ---

- Depends

    Declare dependencies of the service.

    mandatory : no

    syntax : [bracket](#bracket)

    valid values :

    * The name of any valid service.

    The order is of **importance** (!). If fooA depends on fooB and fooB depends on fooC the order needs to be:

    ````
        Depends=(fooA fooB fooC )
    ````

    It is unnecessary to manually define chained sets of dependencies, see [66](66.html#handling-dependencies).

    A service can be commented out by placing the number sign `#` at the begin of the name like this:

    ````
        Depends = ( fooA #fooB fooC )
    ````

    ---

- RequiredBy

    Declare required-by dependencies of the service.

    mandatory : no

    syntax : [bracket](#bracket)

    valid values :

    * The name of any valid service.

    The order is of **importance** (!). If fooA is required by fooB and fooB is required by fooC the order needs to be:

    ````
        RequiredBy=(fooA fooB fooC )
    ````

    It is unnecessary to manually define chained sets of dependencies, see [66](66.html#handling-dependencies).

    A service can be commented out by placing the number sign `#` at the begin of the name like this:

    ````
        Depends = ( fooA #fooB fooC )
    ````

    ---

- OptsDepends

    Declare optional dependencies of the service.

    mandatory : no

    syntax : [bracket](#bracket)

    valid values :

    * The name of any valid service. A service declared as optional dependencies is not mandatory. The parser will look the corresponding service:
        - If enabled, it will warn the user and do nothing.
        - If not, it will try to find the corresponding frontend file.
            - If the frontend service file is found, it will enable it.
            - If it is not found, it will warn the user and do nothing.

    The order is *important* (!). The first service found will be used and the parse process of the field will be stopped. So, you can considere `OptsDepends` field as: "enable one on this service or none".

    A service can be commented out by placing the number sign `#` at the begin of the name like this:

    ````
        OptsDepends = ( fooA #fooB fooC )
    ````

    ---

- Options

    mandatory : no

    syntax : [bracket](#bracket)

    valid values :

    * log : automatically create a logger for the service. This is **default**. The logger will be created even if this options is not specified. If you want to avoid the creation of the logger, prefix the options with an exclamation mark:

        ````
            Options = ( !log )
        ````

        The behavior of the logger can be configured in the corresponding section—see [[Logger]](66-frontend.html#section-logger).

    ---

- Flags


    mandatory : no

    syntax : [bracket](#bracket)

    valid values :

    * down: This will create the file down corresponding to the file down of [s6](https://skarnet.org/software/s6) program.
    * earlier: This set the service as an *earlier* service meaning starts the service as soon as the [scandir](66-scandir.html) is up.

    Once this file was created the default state of the service will be considered down, not up: the service will not automatically be started until it receives a [66 start](66-start.html) command. Without this file the default state of the service will be up and started automatically.

    ---

- Notify

    mandatory : no

    syntax : [uint](#uint)

    valid values :

    * Any valid number.

    This will create the file *notification-fd*. Once this file is created the service supports [readiness notification](https://skarnet.org/software/s6/notifywhenup.html). The value equals the number of the file descriptor that the service writes its readiness notification to. (For instance, it should be 1 if the daemon is [s6-ipcserverd](https://skarnet.org/software/s6/s6-ipcserverd.html) run with the -1 option.) When the service reseive signal and this file is present containing a valid descriptor number, [66](66.html) command will wait for the notification from the service and broadcast its readiness.

    ---

- TimeoutStop

    *Corresponds to the file timeout-finish of [s6](https://skarnet.org/software/s6) program* and used by service of type `classic`.

    mandatory : no

    syntax : [uint](#uint)

    valid values :

    * Any valid number.

    This will create the file *timeout-finish*. Once this file is created the value will equal the number of milliseconds after which the *./finish* script—if it exists—will be killed with a `SIGKILL`. The default is `0` allowing finish scripts to run forever.

    ---

- TimeoutStart

    *Corresponds to the file timeout-kill of [s6](https://skarnet.org/software/s6) program* and used by service of type `classic`.

    mandatory : no

    syntax : [uint](#uint)

    valid values :

    * Any valid number.

    This will create the file *timeout-kill*. Once this file is created and the value is not `0`, then on reception of a [stop](66-stop.html) command—which sends a `SIGTERM` and a `SIGCONT` to the service — a timeout of value in milliseconds is set. If the service is still not dead, after *value* in milliseconds, it will receive a `SIGKILL`. If the file does not exist, or contains `0`, or an invalid value, then the service is never forcibly killed.

    ---


- MaxDeath

    *Corresponds to the file max-death-tally of [s6](https://skarnet.org/software/s6) program*.

    mandatory : no

    syntax : [uint](#uint)

    valid value :

    * Any valid number.

    This will create the file *max-death-tally*. Once this file was created the value will equal the maximum number of service death events that the supervisor will keep track of. If the service dies more than this number of times, the oldest event will be forgotten and the transition ([start](66-start.html) or [stop](66-stop.html)) will be declared as failed. Tracking death events is useful, for example, when throttling service restarts. The value cannot be greater than 4096. Without this file a default of 10 is used.

    ---

- DownSignal

    *Corresponds to the file "down-signal" of [s6](https://skarnet.org/software/s6) program*.

    mandatory : no

    syntax : [uint](#uint)

    valid value :

    * The name or number of a signal.

    This will create the file *down-signal* which is used to kill the supervised process when a [reload](66-reload.html), [restart](66-restart.html) or [stop](66-stop.html) command is used. If the file does not exist `SIGTERM` will be used by default.

    ---

- Hiercopy

    Verbatim copy directories and files on the fly to the main service destination.

    mandatory : no

    syntax : [bracket](#bracket)

    valid values :

    * Any files or directories. It accepts *absolute* or *relative* path.

        **Note**: `66` version must be higher than 0.3.0.1.

- InTree

    mandatory : no

    syntax : [inline](#inline)

    valid values :

    * Any name.

    The service will automatically be activated at the tree name set in the *InTree* key value.

    **Note**: If a corresponding [seed](66-tree.html#seed-files) file exist on your system, its will be used to create and configure the tree.

---

## Section [Start]

This section is *mandatory*. (!)

### Valid *key* names:

- Build

    mandatory : no

    syntax : [inline](#inline)

    valid value :

    * auto : creates the service script file as [execline](https://skarnet.org/software/execline) script. This is the **default**.

        The corresponding file to start the service will automatically be written in [execline](https://skarnet.org/software/execline) format with the `Execute` key value.

    * custom : creates the service script file making a verbatim copy of the `Execute` field of the section. **Do not forget** to set the shebang of the script at `Execute` key.

    ---

- RunAs

    mandatory : no

    syntax : [inline](#inline),[simple-colon](#simple-colon)

    valid value :

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

    **Note**: (!) The service needs to be first started with root if you want to hand over priviliges to a user. Only root can pass on privileges. This field has no effect for other use cases.

    ---

- Execute

    mandatory : yes (!)

    syntax : [bracket](#bracket)

    valid value :

    * The command to execute when starting the service.

    **Note**: The field will be used as is. No changes will be applied at all. It's the responsability of the author to make sure that the content of this field is correct.

---

## Section [Stop]

This section is *optional*.

This section is exactly the same as [[Start]](66-frontend.html#section-start) and shares the same keys. With the exception that it will handle the stop process of the service.

---

## Section [Logger]

This section is *optional*.

It will only have effects if value *log* was **not** prefixed by an exclamation mark to the `Options` key in the [[Main]](66-frontend.html#section-main) section.

This section extends the `Build`, `RunAs`, and `Execute` key fields from [[Start]](66-frontend.html#section-start) and the `TimeoutStop` and `TimeoutStart` key fields from [[Main]](66-frontend.html#section-main) . These are also valid keys for [[Logger]](66-frontend.html#section-logger) and behave the same way they do in the other sections but they can not be specified except for the mandatory key `Build`—see example below. In such case the default behaviour for those key are apply.

Furthermore there are some keys specific to the log.

### Valid *key* names:

- `Build`, `RunAs`, and `Execute` — See [[Start]](66-frontend.html#section-start)
- `TimeoutStop`, `TimeoutStart` — See [[Main]](66-frontend.html#section-main)

    ---

- Destination

    mandatory : no

    syntax : [path](#path)

    valid value :

    * Any valid path on the system.

    The directory where the log file is saved. This directory is automatically created. The current user of the process needs to have sufficient permissions on the destination directory to be able to create it. The default directory is `%%system_log%%/service_name` for root and `$HOME/%%user_log%%/service_name` for any user. The default can also be changed at compile-time by passing the `--with-system-logpath=DIR` option for root and `--with-user-logpath=DIR` for a user to `./configure`.

    ---

- Backup

    mandatory : no

    syntax : [uint](#uint)

    valid value :

    * Any valid number.

    The log directory will keep *value* files. The next log to be saved will replace the oldest file present. By default `3` files are kept.

    ---

- MaxSize

    mandatory : no

    syntax : [uint](#uint)

    valid value :

    * Any valid number.

    A new log file will be created every time the current one approaches *value* bytes. By default, filesize is `1000000`; it cannot be set lower than `4096` or higher than `268435455`.

    ---

- Timestamp

    mandatory : no

    syntax : [inline](#inline)

    valid value :

    * tai

    The logged line will be preceded by a TAI64N timestamp (and a space) before being processed by the next action directive.

    * iso

    The selected line will be preceded by a ISO 8601 timestamp for combined date and time representing local time according to the systems timezone, with a space (not a `T`) between the date and the time and two spaces after the time, before being processed by the next action directive.

    * none

    The logged line will not be preceded by any timestamp.

    The following are two possible examples for the [[Logger]](66-frontend.html#section-logger) section definition.

    ````
        [Logger]
        RunAs = user
        TimeoutStop = 10000
        Destination = /run/log
        Backup = 10
        Timestamp = iso
    ````
    ````
        [Logger]
        Destination = /run/log
    ````

---

## Section [Environment]

This section is *optional*.

A file containing the `key=value` pair(s) will be created by default at `%%service_admconf%%/name_of_service` directory. The default can also be changed at compile-time by passing the `--with-sysadmin-service-conf=DIR` option to `./configure`.

### Valid *key* names:


- Any `key=value` pair

    mandatory : no

    syntax : [pair](#pair)

    valid value :

    * You can define any variables that you want to add to the environment of the service. For example:

    ````
        [Environment]
        dir_run=/run/openntpd
        cmd_args=-d -s
    ````

    The `!` character can precede the value. Ensure **no** space exists between the exclamation mark and the *value*. This action explicitly avoids setting the value of the *key* for the runtime process but only applies it at the start of the service. For intance, the following valid example unset the `key=value` pair `dir_run=!/run/openntpd` from the general environment variables of the service.

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

    Refers to [execl-envfile](execl-envfile.html) for futhers information.

---

## Section [Regex]

This section is *optional*.

It will only have an effect when the service is a `module` type—see the section [Module service creation](66-module-creation.html).

You can use the `@I` string as key field. It will be replaced by the `module` name as you do for instantiated service before applying the regex section.

### Valid *key* names:


- Configure

    mandatory : no

    syntax : [quotes](#quotes)

    valid value :

    * You can define any arguments to pass to the module's configure script.

    ---

- Directories

    mandatory : no

    syntax : [pair](#pair) inside [bracket](#bracket)

    valid value :

    * Any `key=value` pair where key is the regex to search on the directory name and value the replacement of that regex. For example:

    ````
        Directories = ( DM=sddm TRACKER=consolekit )
    ````

    Where the module directory contains two sub-directories named use-DM and by-TRACKER directories. It will be renamed as use-sddm and by-consolekit respectively.

    ---

- Files

    mandatory : no

    syntax : [pair](#pair) inside [bracket](#bracket)

    valid value :

    * Reacts exactly as Directories field but on files name instead of directories name.

    ---

- InFiles

    mandatory : no

    syntax : [colon](#colon) inside [bracket](#bracket)

    valid value :

    * Any valid filename between the double colon with any `key=value` pair where key is the regex to search inside the file and value the replacement of that regex. The double colon **must** be present but the name between it can be omitted. In that case, the `key=value` pair will apply to all files contained on the module directories and to all keys (regex) found inside the same file.For example:

    ````
    InFiles = ( :mount-tmp:args=-o noexec
    ::user=@I )
    ````

    * It replaces first the term `@I` by the name of the module.
    * It opens the file named mount-tmp, search for the args regex and replaces it by the value of the regex.
    * It opens all files found on the module directory and replaces all regex 'user' found by the name of the module in each file.

---

## A word about the Execute key

As described above the `Execute` key can be written in any language as long as you define the key `Build` as custom. For example if you want to write your `Execute` field with bash:

```
    Build = custom
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

The parser creates an exact copy of what it finds between `(` and `)`. This means that regardless of the character found, it retains it. For instance, if you write

```
    Execute = (
    #!/bin/bash echo hello world!
    )
```
the final result will contain a newline at the very beginning corresponding to the newline found between `(` and the definition of the shebang `#!/bin/bash`. In this case, when executing the service, you'll encounter an ***Exec format error*** because the very first line corresponds to a newline instead of the shebang declaration.

To avoid this issue, ALWAYS declare the shebang of your script directly after `(` and without any spaces, tabs, newlines, etc. For example,

```
    Execute = (#!/bin/bash
    echo hello world!
    )
```

When using this sort of custom function `RunAs` has **no effect**. You **must** define with care what you want to happen in a *custom* case.

Furthermore when you set `Build` to auto the parser will take care about the redirection of the ouput of the service when the logger is activated. When setting `Build` to custom though the parser will not do this automatically. You need to explicitly tell it to:

```
    #!/usr/bin/bash
    exec 2>&1
    echo "This script redirects file descriptor 2 to the file descriptor 1"
    echo "Then the logger reads the file descriptor 1 and you have"
    echo "the error of the daemon written into the appropriate file"
```

Finally you need to take care about how you define your environment variable in the section [[Environment]](66-frontend.html#section-environment). When setting `Build` to auto the parser will also take care about the `!` character if you use it. This character will have no **effect** in the case of custom.

This same behavior applies to the [[Logger]](66-frontend.html#section-logger) section. The fields `Destination`, `Backup`, `MaxSize` and `Timestamp` will have **no effect** in a custom case. You need to explicitly define the program to use the logger and the options for it in your `Execute` field.

---

## Prototype of a frontend file

The minimal template is e.g.:

```
    [Main]
    Type = classic
    Version = 0.0.1
    Description = "Template example"
    User = ( root )

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
    TimeoutStart =
    TimeoutStop =
    MaxDeath =
    DownSignal =
    Hiercopy = ()
    InTree =

    [Start]
    Build =
    RunAs =
    Execute = ()

    [Stop]
    Build =
    RunAs =
    Execute = ()

    [Logger]
    Build =
    RunAs =
    Destination =
    Backup =
    MaxSize =
    Timestamp =
    TimeoutStart =
    TimeoutStop =
    Execute = ()

    [Environment]
    mykey=myvalue
    ANOTHERKEY=!antohervalue

    [Regex]
    Configure = ""
    Directories = ()
    Files = ()
    InFiles = ()
```
