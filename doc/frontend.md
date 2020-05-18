title: The 66 Suite: frontend
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# The frontend service file

The [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc) programs each handle and use several kinds of services and different files. It is quite complex to understand and manage the relationship between all those files and services. If you're interested in the details you should read [the documentation for the s6 servicedir](https://skarnet.org/software/s6/servicedir.html) and also about [classic](https://skarnet.org/software/s6/servicedir.html), [oneshot, longrun (also called atomic services),bundle](https://skarnet.org/software/s6-rc/s6-rc-compile.html) and [module](frontend.html#Module service creation) services. The frontend service file of 66 tools allows you to deal with all these different services in a centralized manner in one single place.

By default *66* tools expects to find any service files in `%%service_system%%` and `%%service_adm%%` for root user. For regular users, `$HOME/%%service_user%%` will take priority over the previous ones. Although this can be changed at compile time by passing the `--with-system-service=DIR`, `--with-sysadmin-service=DIR` and `--with-user-service=DIRoption` to `./configure`.

The frontend service file has a format of INI with a specific syntax on the key field. The name of the file usually corresponds to the name of the daemon and does not have any extension or prefix.

The file is made of *sections* which can contain one or more `key value` pairs where the *key* name can contain special characters like `-` (hyphen) or `_` (low line) except the character `@` (commercial at) which is reserved.

You can find a prototype with all valid section and all valid `key=value` pair at the end of this [document](frontend.html#Prototype of a frontend file).

### File names examples

```
    %%service_system%%/dhcpcd
    %%service_system%%/very_long_name
```

### File content example

```
    [main]
    @type = classic
    @description = "ntpd daemon"
    @version = 0.1.0
    @user = ( root )
    @options = ( log env )

    [start]
    @build = auto
    @execute = ( foreground { mkdir -p  -m 0755 ${RUNDIR} } 
    execl-cmdline -s { ntpd ${CMD_ARGS} } )

    [environment]
    dir_run=!/run/openntpd
    cmd_args=!-d -s
```

The parser will **not** accept an empty value. If a *key* is set then the *value* **can not** be empty. Comments are allowed using the number sign `#`. Empty lines are also allowed.

*Key* names are **case sensitive** and can not be modified. Most names should be specific enough to avoid confusion.

The sections can be declared in any order but as a good practice the `[main]` section should be declared first. That way the parser can read the file as fast as possible. 

## Sections

All sections need to be declared with the name written between square brackets `[]` and must be of lowercase letters **only**. This means that special characters, uppercase letters and numbers are not allowed in the name of a section. An entire section can be commented out by placing the number sign `#` in front of the opening square bracket like this: 

```
    #[stop]
```

 The frontend service file allows the following section names:

- [[main]](frontend.html#Section: [main])
- [[start]](frontend.html#Section: [start])
- [[stop]](frontend.html#Section: [stop])
- [[logger]](frontend.html#Section: [logger)
- [[environment]](frontend.html#Section: [environment])
- [[regex]](frontend.html#Section: [regex])

Although a section can be mandatory not all of its key fields must be necessarily so.

---

## Syntax legend

The *value* of a *key* is parsed in a specific format depending on the key. The following is a break down of how to write these syntaxes:

- *inline* : An inline *value*. **Must** be on the same line with its corresponding *key*.

   * Valid syntax:
    
        ````
        @type = classic

        @type=classic
        ````
    
    * **(!)** Invalid syntax:
        
        ````
        @type=
        classic
        ````
    
    ----

- *quotes* : A *value* between double-quotes. **Must** be on the same line with its corresponding *key*.

    * Valid syntax:
    
        ````
        @description = "some awesome description"

        @description="some awesome description"
        ````
    
    * **(!)** Invalid syntax:
        
        ````
        @description=
        "some awesome description"

        @description = "line break inside a double-quote
        is not allowed"
        ````
    
    ----

- *brackets* : Multiple *values* between parentheses `()`. Values need to be separated with a space. A line break can be used instead.

    * Valid syntax:
    
        ````
        @depends = ( fooA fooB fooC )

        @depends=(fooA fooB fooC)

        @depends=(
        fooA
        fooB
        fooC
        )

        @depends= 
        (
        fooA
        fooB
        fooC
        )
        ````
    
    * **(!)** Invalid syntax:
        
        ````
        @depends = (fooAfooBfooC)
        ````
    
    ----

- *uint* : A positive whole number. **Must** be on the same line with its corresponding *key*.

    * Valid syntax:
    
        ````
        @notify = 3

        @notify=3
        ````
    
    * **(!)** Invalid syntax:
        
        ````
        @notify=
        3
        ````
    
    ----

- *path* : An absolute path beginning with a forward slash `/`. **Must** be on the same line with its corresponding *key*.

    * Valid syntax:
    
        ````
        @destination = /etc/66 

        @destination=/etc/66 
        ````
    
    * **(!)** Invalid syntax:
        
        ````
        @destination=/a/very/
        long/path
        ````
    
    ----

- *pair* : same as *inline*.

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
    
- *colon* : A value between double colons followed by a *pair* syntax. **Must** be one by line.

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
    
----

## Section: [main]

This section is *mandatory*. (!)

### Valid *key* names:

- @type
    
    *Corresponds to the file type of [s6-rc](https://skarnet.org/software/s6-rc) programs*.

    Declare the type of the service.

    mandatory : yes (!)

    syntax : inline

    valid values :

    * classic : declares the service as classic.
    * bundle : declares the service as bundle service.  
    * longrun : declares the service as longrun service.
    * oneshot : declares the service as oneshot service.
    * module : declares the service as module service.

    **Note**: If you don't care about dependencies between services or if you don't need specific tasks to get done before running the daemon, "classic" is the best pick.

    ---

- @name
    
    *Corresponds to the name of the service directory of [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc) programs*.

    Name of the service.

    mandatory : no

    syntax : inline

    valid values :

    * This field has no effect except for instantiated services. In such case the name must contain the complete name of the frontend service file.

        For example, the following is valid:
        
        ````
            @name = tty@mine-@I
        ````
        
        where:
        
        ````
            @name = mine-@I
        ````
        
        is not for a frontend service file named tty@.

    ---

- @version
    
    *Without equivalent, this key is unique to 66 tools*.

    Version number of the service.

    mandatory : yes (!)

    syntax : inline

    valid values :

    * Any valid number
    
    ---

- @description
    
    *Without equivalent, this key is unique to 66 tools*.

    A short description of the service.

    mandatory : yes (!)

    syntax : quote

    valid values :

    * Anything you want.

    ---

- @user

    *Without equivalent, this key is unique to 66 tools*.

    Declare the permissions of the service.

    mandatory : yes (!)

    syntax : bracket

    valid values :

    * Any valid user of the system. If you don't know in advance the name of the user who will deal with the service, you can use the term `user`. In that case every user of the system will be able to deal with the service.
    
    (!) Be aware that root is not automatically added. If you don't declare root in this field, you will not be able to use the service even with root privileges.

    ---

- @depends

    *Corresponds to the file dependencies of [s6-rc](https://skarnet.org/software/s6-rc) programs*.

    Declare dependencies of the service.

    mandatory : no—this field has no effect if the type of the service is `classic`.

    syntax : bracket

    valid values :

    * The name of any valid service with type `bundle`, `longrun`, `oneshot` or `module`. Services of type `classic` are not allowed.
    
    The order is of **importance** (!). If fooA depends on fooB and fooB depends on fooC the order needs to be:
    
    ````
        @depends=(fooA fooB fooC )
    ````
    
    It is unnecessary to manually define chained sets of dependencies. The parser will properly handle this for you. For example, if fooA depends on fooB—no matter what the underlying implementation of fooB is, and although the current implementation of fooB depends on fooC—you should just put fooB in the @depends key field of fooA. When starting the set, [66-enable](66-enable.html) will parse and enable fooA, fooB and fooC and [66-start](66-start.html) will start fooC first, then fooB, then fooA. If the underlying implementation of fooB changes at any moment and does not depend on fooC anymore, you will just have to modify the *@depends* field for fooB. Beware though that if fooA depends on fooC, you need to add both fooB and fooC to the dependencies of fooA.

    A service can be commented out by placing the number sign `#` at the begin of the name like this:
        
    ````
        @depends = ( fooA #fooB fooC )
    ````

    ---
    
- @optsdepends

    *Without equivalent, this key is unique to 66 tools*.

    Declare optional dependencies of the service.

    mandatory : no—this field has no effect if the type of the service is `classic` or `bundle`.

    syntax : bracket

    valid values :

    * The name of any valid service with type `bundle`, `longrun`, `oneshot` or `module`. Services of type `classic` are not allowed. A service declared as optional dependencies is not mandatory. The parser will look at all trees if the corresponding service is already enabled:
        - If enabled, it will warn the user and do nothing.
        - If not, it will try to find the corresponding frontend file.
            - If the frontend service file is found, it will enable it.
            - If it is not found, it will warn the user and do nothing.

    The order is *important* (!). The first service found will be used and the parse process of the field will be stopped. So, you can considere *@optsdepends* field as: "enable one on this service or none".

    @optsdepends only affects the enable process. The start process will not consider optional dependencies. If fooA on treeA has an optional dependency fooB which is declared on treeB, it's the responsibility of the sysadmin to start first treeB then treeA. [66-intree](66-intree.html) can give you the start order with the field `Start after`.

    A service can be commented out by placing the number sign `#` at the begin of the name like this:
        
    ````
        @optsdepends = ( fooA #fooB fooC )
    ````

    ---

- @extdepends

    *Without equivalent, this key is unique to 66 tools*.

    Declare external dependencies of the service.

    mandatory : no—this field has no effect if the type of the service is `classic` or `bundle`.

    syntax : bracket

    valid values :

    * The name of any valid service with type `bundle`, `longrun`, `oneshot` or `module`. Services of type `classic` are not allowed. A service declared as an external dependencies is mandatory. The parser will search through all trees whether the corresponding service is already enabled:
       - If enabled, it will warn the user and do nothing.
       - If not, it will try to find the corresponding frontend file.
            - If the frontend service file is found, it will enable it.
            - If not, the process is stopped and [66-enable](66-enable.html) exits 111.

    This process is made for all services declared in the field.


    So, you can consider *@extdepends* field as: "enable the service if it is not already declared on a tree".

    *@extdepends* only affects the enable process. The start process will not consider external dependencies. If fooA on treeA has an external dependency fooB which is declared on treeB, it's the responsibility of the sysadmin to start first treeB then treeA. [66-intree](66-intree.html) will give you the start order with the field `Start after`.

    A service can be commented out by placing the number sign `#` at the begin of the name like this:
    
    ````
        @extdepends = ( fooA #fooB fooC )
    ````
    
    ---
    
- @contents

    *Corresponds to the file contents of [s6-rc](https://skarnet.org/software/s6-rc) programs*.

    Declare the contents of a bundle service.

    mandatory : yes (!)—for services of type `bundle`. Not allowed for all other services type.

    syntax : bracket

    valid values :

    * The name of any valid service of type `bundle`, `longrun`, `oneshot` or `module`. Services of type `classic` are not allowed.

    A service can be commented out by placing the number sign `#` at the begin of the name like this:
    
    ````
        @contents = ( fooA #fooB fooC)
    ````
    
    ---
    
- @options
    
    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : bracket

    valid values :

    * log : automatically create a logger for the service. The behavior of the logger can be adjusted in the corresponding section—see [[logger]](frontend.html#Section: [logger]).
    * env : enable the use of the [[environment]](frontend.html#Section: [environment]) section for the service.
    pipeline : automatically create a pipeline between the service and the logger. For more information read the [s6-rc](https://skarnet.org/software/s6-rc) documentation.

    **Note**: The funnel feature of pipelining is not implemented yet.
    
    ---
    
- @flags
    
    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : bracket

    valid values :

    * nosetsid : Corresponds to the file *nosetsid* of [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc) programs

    This will create the file nosetsid

    Once this file was created the service will be run in the same process group as the supervisor of the service ([s6-supervise](https://skarnet.org/software/s6/s6-supervise.html)). Without this file the service will have its own process group and is started as a session leader.
    
    * down : Corresponds to the file *down* of [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc) programs.
    This will create the file down

    Once this file was created the default state of the service will be considered down, not up: the service will not automatically be started until it receives a [66-start](66-start.html) command. Without this file the default state of the service will be up and started automatically.

    ---
    
- @notify
    
    *Corresponds to the file notification-fd of [s6-rc](https://skarnet.org/software/s6-rc) programs*.
    
    mandatory : no

    syntax : uint

    valid values :

    * Any valid number.

    This will create the file *notification-fd*. Once this file was created the service supports [readiness notification](https://skarnet.org/software/s6/notifywhenup.html). The value equals the number of the file descriptor that the service writes its readiness notification to. (For instance, it should be 1 if the daemon is [s6-ipcserverd](https://skarnet.org/software/s6/s6-ipcserverd.html) run with the -1 option.) When the service is started or restarted and this file is present and contains a valid descriptor number, [66-start](66-start.html) will wait for the notification from the service and broadcast readiness, i.e. any `66-svctl -U` process will be triggered.

    ---

- @timeout-finish
    
    *Corresponds to the file timeout-finish of [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc) programs*.

    mandatory : no

    syntax : uint

    valid values :

    * Any valid number.

    This will create the file *timeout-finish*. Once this file was created the value will equal the number of milliseconds after which the *./finish* script—if it exists—will be killed with a `SIGKILL`. The default is `5000`; finish scripts are killed if they're still alive after `5` seconds. A value of `0` allows finish scripts to run forever.

    ---

- @timeout-kill

    *Corresponds to the file timeout-kill of [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc) programs*.

    mandatory : no

    syntax : uint

    valid values :

    * Any valid number.

    This will create the file *timeout-kill*. Once this file was created and the value is not `0`, then on reception of a [66-stop](66-stop.html) command—which sends a `SIGTERM` and a `SIGCONT` to the service—a timeout of value milliseconds is set. If the service is still not dead after *value* milliseconds it will receive a `SIGKILL`. If the file does not exist, or contains 0 or an invalid value, then the service is never forcibly killed (unless, of course, a [s6-svc -k](https://skarnet.org/software/s6/s6-svc.html) command is sent).

    ---
    
- @timeout-up

    *Corresponds to the file timeout-up of [s6-rc](https://skarnet.org/software/s6-rc) programs*.

    mandatory : no

    syntax : uint

    valid value :

    * Any valid number.

    This will create the file *timeout-up*. Once this file was created the *value* will equal the maximum number of milliseconds that [66-start](66-start.html) will wait for successful completion of the service start. If starting the service takes longer than this value, [66-start](66-start.html) will declare the transition a failure. If the value is `0`, no timeout is defined and [66-start](66-start.html) will wait for the service to start until the *maxdeath* is reached. Without this file a value of `3000` (`3` seconds) will be taken by default.

    ---
    
- @timeout-down

    *Corresponds to the file timeout-down of [s6-rc](https://skarnet.org/software/s6-rc) programs*.

    mandatory : no

    syntax : uint

    valid value :

    * Any valid number.

    This will create the file *timeout-down*. Once this file was created the value will equal the maximum number of milliseconds [66-stop](66-stop.html) will wait for successful completion of the service stop. If starting the service takes longer than this value, [66-stop](66-stop.html) will declare the transition a failure. If the value is `0`, no timeout is defined and [66-stop](66-stop.html) will wait for the service to start until the *maxdeath* is reached. Without this file a value of `3000` (`3` seconds) will be taken by default.

    ---
    
- @maxdeath
    
    *Corresponds to the file max-death-tally of [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc) programs.

    mandatory : no

    syntax : uint

    valid value :

    * Any valid number.

    This will create the file *max-death-tally*. Once this file was created the value will equal the maximum number of service death events that the supervisor will keep track of. If the service dies more than this number of times, the oldest event will be forgotten and the transition ([66-start](66-start.html) or [66-stop](66-stop.html)) will be declared as failed. Tracking death events is useful, for example, when throttling service restarts. The value cannot be greater than 4096. Without this file a default of 3 is used.

    ---
    
- @down-signal
    
    *Corresponds to the file "down-signal" of [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc) programs*.

    mandatory : no

    syntax : uint

    valid value :

    * The name or number of a signal.

    This will create the file *down-signal* which is used to kill the supervised process when a [66-start -r](66-start.html) or [66-stop](66-stop.html) command is used. If the file does not exist `SIGTERM` will be used by default.

    ---
    
- @hiercopy

    *Without equivalent, this key is unique to 66 tools*.

    Verbatim copy directories and files on the fly to the main service destination.

    mandatory : no

    syntax : bracket

    valid values :

    * Any files or directories. It accepts *absolute* or *relative* path. 
    
        **Note** : 66 version must be higher than 0.3.0.1.
    
---

## Section: [start]

This section is *mandatory*. (!)

### Valid *key* names:

- @build
    
    *Without equivalent, this key is unique to 66 tools*.

    mandatory : yes (!)

    syntax : inline

    valid value :
        
    * auto : creates the service script file as [execline](https://skarnet.org/software/execline) script.

    The corresponding file to start the service will automatically be written in [execline](https://skarnet.org/software/execline) format with the *@execute* key value.
        
    * custom : creates the service script file in the language set in the *@shebang* key value.

    The corresponding file to start the service will be written in the language set in the *@shebang* key value.

    ---

- @runas
  
    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : inline

    valid value :
    
    * Any valid user set on the system

    This will pass the privileges of the service to the given user before starting the last command of the service.

    **Note**: (!) The service needs to be first started with root if you want to hand over priviliges to a user. Only root can pass on privileges. This field has no effect for other use cases.

    ---
    
- @shebang
  
    *Without equivalent, this key is unique to 66 tools*.

    mandatory : yes (!)—if the *@build* key is set to custom.

    syntax : quotes, path

    valid value :
        
    * A valid path to an interpreter installed on your system.

    This will set the language that will be used to read and write the *@execute* key value.
    
    ---
    
- @execute
  
    *Corresponds to the file run for a classic or longrun service and to the file up for a oneshot service*.

    mandatory : yes (!)

    syntax : bracket

    valid value :
       
    * The command to execute when starting the service.

    **Note**: The field will be used as is. No changes will be applied at all. It's the responsability of the author to make sure that the content of this field is correct.

---

## Section: [stop]

This section is *optional*.

This section is exactly the same as [[start]](frontend.html#Section: [start]) and shares the same keys. With the exception that it will only be considered when creating the file *finish* for a `classic` or `longrun` service and when creating the file *down* for a `oneshot` service to create its content.

---

## Section: [logger]

This section is *optional*.

The value *log* must be added to the *@options* key in the [[main]](frontend.html#Section: [main]) section for [[logger]](frontend.html#Section: [logger]) to have any effect.

This section extends the *@build*, *@runas*, *@shebang* and *@execute* key fields from [[start]](frontend.html#Section: [start])/[[stop]](frontend.html#Section: [stop]) and the *@timeout-finish* and *@timeout-kill* key fields from [[main]](frontend.html#Section: [main]) . These are also valid keys for [[logger]](frontend.html#Section: [logger]) and behave the same way they do in the other sections but they can not be specified except for the mandatory key *@build*—see example below. In such case the default behaviour for those key are apply.

Furthermore there are some keys specific to the log.

### Valid *key* names:

- *@build*, *@runas*, *@shebang*, *@execute* — See [[start]](frontend.html#Section: [start])
- *@timeout-finish*, *@timeout-kill* — See [[main]](frontend.html#Section: [main])

    ---

- @destination
  
    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : path

    valid value :
    
    * Any valid path on the system.

    The directory where to save the log file. This directory is automatically created. The current user of the process needs to have sufficient permissions on the destination directory to be able to create it. The default directory is `%%system_log%%/service_name` for root and `$HOME/%%user_log%%/service_name` for any regular user. The default can also be changed at compile-time by passing the `--with-system-logpath=DIR` option for root and `--with-user-logpath=DIR` for a user to `./configure`.
    
    ---
    
- @backup
    
    Without equivalent, this key is unique to 66 tools.

    mandatory : no

    syntax : uint

    valid value :
    
    * Any valid number.

    The log directory will keep *value* files. The next log to be saved will replace the oldest file present. By default `3` files are kept.
    
    ---
    
- @maxsize

    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : uint

    valid value :
    
    * Any valid number.

    A new log file will be created every time the current one approaches *value* bytes. By default, filesize is `1000000`; it cannot be set lower than `4096` or higher than `268435455`.

    ---
    
- @timestamp

    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : inline

    valid value :
        
    * tai

    The logged line will be preceded by a TAI64N timestamp (and a space) before being processed by the next action directive.
    
    * iso

    The selected line will be preceded by a ISO 8601 timestamp for combined date and time representing local time according to the systems timezone, with a space (not a `T`) between the date and the time and two spaces after the time, before being processed by the next action directive.

    The following are two possible examples for the [[logger]](frontend.html#Section: [logger]) section definition.

    ````
        [logger]
        @build = auto
        @runas = user
        @timeout-finish = 10000

        @destination = /run/log
        @backup = 10
        @timestamp = iso

        [logger]
        @build = auto
        @timestamp = iso
    ````

---

## Section: [environment]

This section is *optional*.

It will only have an effect when the value env is added to the *@options* key in the [[main]](frontend.html#Section: [main]) section.

A file contained the `key=value` pair(s) will be created by default at `%%service_admconf%%/name_of_service` directory. The default can also be changed at compile-time by passing the `--with-sysadmin-service-conf=DIR` option to `./configure`.

### Valid *key* names:


- Any `key=value` pair
  
    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : pair

    valid value :

    * You can define any variables that you want to add to the environment of the service. For example:
    
    ````
        [environment]
        dir_run=/run/openntpd
        cmd_args=-d -s
    ````
    
    The `!` character can be put in front of the value like this :
    
    ````
        [environment]
        dir_run=!/run/openntpd
        cmd_args=!-d -s
    ````
    
    This will explicitly **not** set the value of the key for the runtime process but only at the start of the service. In this example the `key=value` pair passed to the command line does not need to be present in the general environment variable of the service.

---

## Section: [regex]

This section is *optional*.

It will only have an effect when the service is a `module` type—see the section [Module service creation](module-service.html).

You can use the `@I` string as key field. It will be replaced by the `module` name as you do for instantiated service before applying the regex section.

### Valid *key* names:


- @configure
  
    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : quotes

    valid value :

    * You can define any arguments to pass to the module's configure script.
    
    ---

- @directories
  
    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : pair inside bracket

    valid value :

    * Any `key=value` pair where key is the regex to search on the directory name and value the replacement of that regex. For example:
    
    ````
        @directories = ( DM=sddm TRACKER=consolekit )
    ````

    Where the module directory contains two sub-directories named use-DM and by-TRACKER directories. It will be renamed as use-sddm and by-consolekit respectively.

    ---
    
- @files
    
    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : pair inside bracket

    valid value :

    * Reacts exactly as @directories field but on filename instead of directories name.

    ---

- @infiles
  
    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : colon

    valid value :

    * Any valid filename between the double colon with any `key=value` pair where key is the regex to search inside the file and value the replacement of that regex. The double colon **must** be present but the name between it can be omitted. In that case, the `key=value` pair will apply to all files contained on the module directories and to all keys (regex) found inside the same file.For example:

    ````
    @infiles = ( :mount-tmp:args=-o noexec
    ::user=@I )
    ````        

    * It replaces first the term `@I` by the name of the module.
    * It opens the file named mount-tmp, search for the args regex and replaces it by the value of the regex.
    * It opens all files found on the module directory and replaces all regex 'user' found by the name of the module in each file.

- @addservices

    *Without equivalent, this key is unique to 66 tools*.

    mandatory : no

    syntax : bracket

    valid value :

    * The name of any valid service with type `bundle`, `longrun`, `oneshot` or `module`. The corresponding frontend file **must** exist on your system. 

    A service can be commented out by placing the number sign `#` at the begin of the name like this:
        
    ````
        @addservices = ( fooA #fooB fooC )
    ````


---

## A word about the @execute key

As described above the *@execute* key can be written in any language as long as you define the key *@build* as custom and the *@shebang* key to the language interpreter to use. For example if you want to write your *@execute* field with bash:

```
    @build = custom
    @shebang = "/usr/bin/bash"
    @execute = ( 
        echo "This script displays available services"
        for i in $(ls %%service_system%%); do
            echo "daemon : ${i} is available"
        done
    )
```

This is an unnecessary example but it shows how to construct this use case. The parser will put your *@shebang* at the beginning of the script and copy the contents of the *@execute* field. So, the resulting file will be :

```
    #!/usr/bin/bash
    echo "This script displays available services"
    for i in $(ls %%service_system%%); do
        echo "daemon : ${i} is available"
    done
```

When using this sort of custom function *@runas* has **no effect**. You **must** define with care what you want to happen in a *custom* case.

Furthermore when you set *@build* to auto the parser will take care about the redirection of the ouput of the service when the logger is activated. When setting *@build* to custom though the parser will not do this automatically. You need to explicitly tell it to:

```
    #!/usr/bin/bash
    exec 2>&1
    echo "This script redirects file descriptor 2 to the file descriptor 1"
    echo "Then the logger reads the file descriptor 1 and you have"
    echo "the error of the daemon written into the appropriate file"
```

Moreover, for `oneshot` type the *@shebang* needs to contain the interpreter options as below:

```
    @build = custom
    @shebang = "/usr/bin/bash -c"
    @execute = ( echo "this is a oneshot service with a correct shebang definition" )
```

Finally you need to take care about how you define your environment variable in the section [[environment]](frontend.html#Section: [environment]). When setting *@build* to auto the parser will also take care about the `!` character if you use it. This character will have no **effect** in the case of custom.

This same behavior applies to the [[logger]](frontend.html#Section: [logger]) section. The fields *@destination*, *@backup*, *@maxsize* and *@timestamp* will have **no effect** in a custom case. You need to explicitly define the program to use the logger and the options for it in your *@execute* field.

---

## Prototype of a frontend file

This prototype contain all valid section with all valid `key=value` pair.

```
    [main]
    @type = classic,bundle,longrun,oneshot,module
    @name = 
    @version =
    @description = ""
    @depends = ()
    @optsdepends = ()
    @extdepends = ()
    @contents = ()
    @options = ( log env pipeline )
    @flags = ( down nosetsid )
    @notify = 
    @user = ()
    @timeout-finish =
    @timeout-kill =
    @timeout-up =
    @timeout-down =
    @maxdeath = 
    @down-signal =
    @hiercopy = ()
    
    [start]
    @build = auto,custom
    @runas = 
    @shebang = "/path"
    @execute = ()
    
    [stop]
    @build = auto,custom
    @runas = 
    @shebang = "/path"
    @execute = ()
    
    [logger]
    @build = auto,custom
    @runas = 
    @shebang = "/path"
    @destination = /path
    @backup = 
    @maxsize = 
    @timestamp = 
    @timeout-finish = 
    @timeout-kill =
    @execute = ()
    
    [environment]
    mykey=myvalue
    ANOTHERKEY=!antohervalue
    
    [regex]
    @configure = "arguments to pass to configure script"
    @directories = ( key=value key=value )
    @files = ( key=value key=value )
    @infiles = ( :filename:key=value ::key=value )
    @addservices = ()
```
