66-frontend(5)

# NAME

The service file format for 66

# SYNTAX

The *s6* and *s6-rc* programs each handle and use several kinds of services
and different files. It is quite complex to understand and manage the 
relationship between all those files and services. If you're interested 
in the details you should read the documentation for the s6 servicedir 
and also about _classic_,_oneshot_,_longrun_ (also called _atomic_ services), 
_bundle_ and _module_(see module below) services. 
The frontend service file of 66 tools allows you to deal with all these 
different services in a centralized manner in one single place.

By default *66* tools expect to find any service file in
*%%service_system%%* and *%%service_adm%%* for root user. For the non
root users, *$HOME/%%service_user%%* will take priority over the previous
ones. Although this can be changed at compile time by passing the
*--with-system-service=*_DIR_, *--with-sysadmin-service=*_DIR_ and 
*--with-user-service=*_DIR_ option to *./configure*.

The frontend service file has a format of INI with a specific syntax on the key
field. 
The name of the file usually corresponds to the name of the daemon and does not
have any extension or prefix.

The file is made of _sections_ which can contain one or more _key value_ pairs
where the _key_ name can contain special characters like *-* (_hyphen_) or
*\_* (_low line_) except the character *@* (_commercial at_) which is reserved.

## File name examples

```
%%service_system%%/dhcpcd
%%service_system%%/very_long_name
```

## File content example

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

The parser will not accept any empty _value_. If a _key_ is set, the _value_
cannot be empty. Comments are allowed using the number sign *#*. Empty lines
are also allowed.

_Key names_ are *case sensitive* and _cannot be modified_. Most names should be
specific enough to avoid confusion.

The sections can be declared in any order but as a good practice 
the *[main]* section should be declared first. That way the parser
can read the file as fast as possible.

## SECTIONS

All sections need to be declared with the name written between square brackets
*[]* and must be of lowercase letters *only*. This means that special
characters, uppercase letters and numbers are not allowed in the name of a
section. An _entire_ section can be commented out by placing
the number sign *#* in front of the opening square bracket like this:

```

   #[stop]

```

The frontend service file allows the following section names:

- [main]
- [start]
- [stop]
- [logger]
- [environment]
- [regex]

Although a section can be mandatory not all of its key fields must be necessarily so.

## SYNTAX LEGEND

The _value_ of a _key_ is parsed in a specific format depending on the key.
The following is a break down of how to write these syntax:

- *inline*
	An inline _value_. Must be in the same line with its corresponding _key_.

	Valid syntax:
```
@type = classic
@type=classic
```

	Invalid syntax:
```
@type=
classic
```

- *quotes*
	A _value_ between double-quotes. Must be on the same line with its 
	corresponding _key_.

	Valid syntax:
```
@description = "some awesome description"
@description="some awesome description"
```

	Invalid syntax:
```
@description=
"some awesome description"
@description = "line break inside a double quote
is not allowed"
```

- *bracket*
	Multiple _values_ between parentheses'()'. Values need to be separated
	with a space. A line break can be used instead.

	Valid syntax:
```
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
```

	Invalid syntax:
```
@depends = (fooAfooBfooC)
```

- *uint*
	A positive whole number. Must be on the same line with its corresponding
	_key_.

	Valid syntax:
```
@notify = 3
@notify=3
```

	Invalid syntax:
```
@notify=
3
```

- *path*
	An absolute path beginning with a forward slash */*. Must be on the same
	line with its corresponding _key_.

	Valid syntax:
```
@destination = /etc/66
@destination=/etc/66
```

	Invalid syntax:
```
@destination=/a/very/
long/path
```

- *pair*
	Same as inline.

	Valid syntax:
```
MYKEY = MYVALUE
anotherkey=anothervalue
anotherkey=where_value=/can_contain/equal/Character
```

	Invalid syntax:
```
MYKEY=
MYVALUE
```

- *colon*
	A _value_ between double colons followed by a _pair_ syntax. Must be one
	by line.

	Valid syntax:
```
::key=value
:filename:key=value
```

	Invalid syntax:
```
::MYKEY=
MYVALUE

::
MYKEY=MYVALUE

::key=value :filename:anotherkey=anothervalue
```

## [main]

This section is *mandatory*.

Valid _key_ names:

*@type*++
	Declare the type of the service.++ 
	_mandatory_: *yes*++
	_syntax_: *inline*++
	_valid values_:++
		*classic*, declares the service as _classic_.++
		*bundle*, declares the service as _bundle_ service.++
		*longrun*, declares the service as _longrun_ service.++
		*oneshot*, declares the service as _oneshot_ service.++
		*module*, declares the service as _module_ service.

	Corresponds to the file *type* of s6-rc programs.

	*Note*: If you don't care about dependencies between services or if you
	don't need specific tasks to get done before running the daemon,
	_classic_ is the best pick.

*@name*++
	Name of the service.++
	_mandatory_: *no*++
	_syntax_: *inline*++
	_valid values_:++
    
	This field has *no effect* except for instantiated services.
	In such case the name *must* contain the complete name of the 
	frontend service file.
	For example, the following is valid:

	@name = tty@mine-@I

	where:

	@name = mine-@I

	is not for a frontend service file named tty@.

	Corresponds to the _name_ of the service directory of *s6* and *s6-rc*
	programs.

*@version*++
	Version number of the service.++
	_mandatory_: *yes*++
	_syntax_: *inline*++
    _valid values_:++
		Any valid number

	Without equivalent, this key is unique to *66* tools.

*@description*++
	A short description of the service.++
	_mandatory_: *yes*++
	_syntax_: *quote*++
	_valid values_:++
		Anything you want.

	Without equivalent, this key is unique to *66* tools.

*@user*++
	Declare the permissions of the service.++
	_mandatory_: *yes*++
	_syntax_: *bracket*++
	_valid values_:++
		Any valid user of the system.

	If you don't know in advance the name
	of the user who will deal with the service, you can use the term
	*user*. In that case every user of the system will be able to deal
	with the service.

	Be aware that _root is not automatically added_.++
	If you don't declare *root* in this field, you will _not_ be able
	to use the service even with root privileges.

	Without equivalent, this key is unique to *66* tools.

*@depends*++
	Declare dependencies of the service.++
	_mandatory_: *no*. This field has no effect if the type++
	of the service is _classic_ or _bundle_.
	_syntax_: *bracket*++
	_valid values_:++
		The _name_ of any valid service with type *bundle*, *longrun*,++
		*oneshot* or *module*. Services of type *classic* are not allowed.

	The order is of importance. If fooA depends on fooB and fooB
	depends on fooC the order needs to be: 

	@depends=(fooA fooB fooC)

	It is unnecessary to manually define chained sets of dependencies.
	The parser will properly handle this for you. For example,
	if fooA depends on fooB - no matter the underlying implementation of
	fooB, and although the current implementation of fooB depends on fooC -
	you should just put fooB in the *@depends* key field of fooA. When
	starting the set, *66-enable*(1) will parse and enable fooA, fooB and
	fooC and 66-start will start fooC first, then fooB, then fooA.
	If the underlying implementation of fooB changes at any moment and does
	not depend on fooC anymore, you will just have to modify the *@depends*
	field for fooB.
	Beware though that if fooA depends on fooC, you need to add both fooB
	and fooC to the dependencies of fooA.

	A service can be commented out by placing the number sign '#' at the
	begin of the name like this:

	@depends = ( fooA #fooB fooC )

	Corresponds to the file *dependencies* of *s6-rc* programs.

*@optsdepends*++
	Declare optional dependencies of the service.++
	_mandatory_: *no*. This field has no effect if the type++
	of the service is _classic_ or _bundle_.
	_syntax_: *bracket*++
	_valid values_:++
		The _name_ of any valid service with type *bundle*, *longrun*,++
		*oneshot* or *module*. Services of type *classic* are not allowed.

	A service declared as optional dependencies is not mandatory.
	The parser will look at all trees if the corresponding service is
	already enabled:++
		If enabled, it will warn the user and do nothing.++
		If not, it will try to find the corresponding frontend file.++
			If the frontend service file is found, it will enable it.++
			If it not found it, it will warn the user and do nothing.

	The *order* is of importance. The first service found will be used 
	and the parse process of the field will be stopped.
	So, you can considere *@optsdepends* field as: 
	"enable one on this service or none".

	@optsdepends only affects the enable process. The start process will
	consider optional dependencies. If fooA on treeA have an optional 
	dependency fooB which is declared on treeB, it's the responsability 
	of the sysadmin to start first treeB then treeA. *66-intree* can give
	you	the start order with the field *Start after*.

	A service can be commented out by placing the number sign '#' at the
	begin of the name like this:

	@optsdepends = ( fooA #fooB fooC )

	Without equivalent, this key is unique to *66* tools.

*@extdepends*++
	Declare external dependencies of the service.++
	_mandatory_: *no*. This field has no effect if the type++
	of the service is _classic_ or _bundle_.
	_syntax_: *bracket*++
	_valid values_:++
		The _name_ of any valid service with type *bundle*, *longrun* or++
		*oneshot*. Services of type *classic* are not allowed.

	A service declared as external dependencies is mandatory.
	The parser will search through all trees whether the corresponding service is
	already enabled:++
		If enabled, it will warn the user and do nothing.++
		If not, it will try to find the corresponding frontend file.++
			If the frontend service file is found, it will enable it.++
			If not, the process is stopped and *66-enable* exit 111.

	This process is made for all services declared in the field.

	So, you can considere @extdepends field as: 
	"enable the service if it not already declared on a tree".
	
	@extdepends only affects the enable process. The start process will not consider
	external dependencies. If fooA on treeA has an external dependency fooB which is
	declared on treeB, it's the responsibility of the sysadmin to start first
	treeB then treeA. *66-intree* will give you
	the start order with the field *Start after*.

	A service can be commented out by placing the number sign '#' at the
	begin of the name like this:

	@extdepends = ( fooA #fooB fooC )

	Without equivalent, this key is unique to *66* tools.

*@contents*++
	Declare the contents of a bundle service.++
	_mandatory_: *yes* for services of type *bundle*.++
	Not allowed for all other services type.++
	_syntax_: *bracket*++
	_valid values_:++
		The name of any valid service of type *bundle*, *longrun* or++
		*oneshot*. Services of type *classic* are not allowed.

	Corresponds to the file *contents* of *s6-rc* programs.

*@options*++
	_mandatory_: *no*++
	_syntax_: *bracket*++
	_valid values_:++
		*log*: automatically create a logger for the service. The behavior++
			of the logger can be adjusted in the corresponding section.++
		*env*: enable the use of the *[environment]* section for the service.++
		*pipeline*: automatically create a pipeline between the service and++
			the logger. For more information read the s6-rc documentation.++
			*Note*: The _funnel_ feature of pipelining is not implemented yet.

	Without equivalent, this key is unique to *66* tools.

*@flags*++
	_mandatory_: *no*++
	_syntax_: *bracket*++
	_valid values_:++
		*nosetsid*:++
			This will create the file *nosetsid*.++
			Once this file was created the service will be run in the same++
			process group as the supervisor of the service (s6-supervise).++
			Without this file the service will have its own process group and++
			is started as a session leader.++
			Corresponds to the file *nosetsid* of *s6-rc* and *s6* programs.++
		*down*++
			This will create the file *down*.++
			Once this file was created the default state of the service++
			will be considered *down*, not *up*: the service will not++
			automatically be started until it receives a *66-start*(1)++
			command. Without this file the default state of the service++
			will be *up* and started automatically.++
			Corresponds to the file *down* of *s6-rc* and *s6* programs.

*@notify*++
	_mandatory_: *no*++
	_syntax_: *uint*++
	_valid values_:++
		Any valid number.

	This will create the file *notification-fd*.
	Once this file was created the service supports readiness
	notification. The _value_ equals the number of the file descriptor
	that the service writes its readiness notification to.
	(For instance, it should be 1 if the daemon is *s6-ipcserverd*
	run with the *-1* option.) When the service is started or restarted
	and this file is present and contains a valid descriptor number,
	*66-start*(1) will wait for the notification from the service and
	broadcast readiness, i.e. any *66-svctl -U* process will be
	triggered.

	Corresponds to the file *notification-fd* of *s6-rc* programs.

*@timeout-finish*++
	_mandatory_: *no*++
	_syntax_: *uint*++
	_valid values_:++
		Any valid number.

	This will create the file *timeout-finish*.
	Once this file was created the _value_ will equal the number of
	milliseconds after which the *./finish* script; if it exists; will
	be killed with a *SIGKILL*. The default is *5000*; finish scripts
	are killed if they're still alive after 5 seconds. A value of
	*0* allows finish scripts to run forever.

	Corresponds to the file *timeout-finish* of *s6-rc* and *s6* programs.

*@timeout-kill*++
	_mandatory_: *no*++
	_syntax_: *uint*++
	_valid values_:++
		Any valid number.

	This will create the file *timeout-kill*.
	Once this file was created and the _value_ is not *0*, then one
	reception of a *66-stop*(1) command; which sends a *SIGTERM* and a
	*SIGCONT* to the service; a timeout of _value_ milliseconds is set.
	If the service is still not dead after _value_ milliseconds it will
	receive a *SIGKILL*. 
	If the file does not exist, or contains *0* or an invalid value,
	then the service is never forcibly killed (unless, of course, a
	*s6-svc -k* command is sent).

	Corresponds to the file *timeout-kill* of *s6-rc* and *s6* programs.

*@timeout-up*++
	_mandatory_: *no*++
	_syntax_: *uint*++
	_valid value_:++
		Any valid number.

	This will create the file *timeout-up*.
	Once this file was created the _value_ will equal the maximum
	number of milliseconds that *66-start*(1) will wait for successful
	completion of the service start. If starting the service takes longer
	than this _value_, *66-start*(1) will declare the transition a failure.
	If the _value_ is *0*, no timeout is defined and *66-start*(1) will
	wait for the service to start until the *maxdeath* is reached. Without
	this file a _value_ of *3000* (3 seconds) will be taken by default.

	Corresponds to the file *timeout-up* of *s6-rc* programs.

*@timeout-down*++
	_mandatory_: *no*++
	_syntax_: *uint*++
	_valid value_:++
		Any valid number.

	This will create the file *timeout-down*. 
	Once this file was created the _value_ will equal the maximum number of
	milliseconds *66-stop*(1) will wait for successful completion of the
	service stop. If starting the service takes longer than this _value_,
	*66-stop*(1) will declare the transition a failure. If the _value_ is *0*,
	no timeout is defined and *66-stop*(1) will wait for the service to start
	until the *maxdeath* is reached. Without this file a _value_ of *3000* (3
	seconds) will be taken by default.

	Corresponds to the file *timeout-down* of *s6-rc* programs.

*@maxdeath*++
	_mandatory_: *no*++
	_syntax_: *uint*++
	_valid value_:++
		Any valid number.

	This will create the file *max-death-tally*. 
	Once this file was created the _value_ will equal the maximum number of
	service death events that the supervisor will keep track of. If the service
	dies more than this number of times, the oldest event will be forgotten and
	the transition (*66-start*(1) or *66-stop*(1)) will be declared as failed.
	Tracking death events is useful, for example, when throttling service
	restarts. The _value_ cannot be greater than *4096*. Without this file a default of *3* is used.

	Corresponds to the file *max-death-tally* of *s6-rc* and *s6* programs.

*@down-signal*++
	_mandatory_: *no*++
	_syntax_: *uint*++
	_valid value_:++
		The name or number of a signal.

	This will create the file *down-signal* which is used to kill the
	supervised process when a *66-start -r* or *66-stop* command is used.
	If the file does not exist SIGTERM will be used by default.

	Corresponds to the file *down-signal* of *s6-rc* and *s6* programs.

*@hiercopy*
	Copy the corresponding name to the service directory destination.
	_mandatory_: *no*
	_syntax_: *bracket*
	_valid values_:
		Any elements name at the directory frontend service file. If the
		frontend service file found at %%service_system%%/foo/foo where
		directory %%service_system%%foo contains
		%%service_system%%/foo/data as directory and
		%%service_system%%/foo/scripts, use @hiercopy=(data scripts) to copy
		the directory data and the file scripts to the service directoy
		destination.

	Without equivalent, this key is unique to *66* tools.</h5>

## [start]

This section is *mandatory*.

Valid _key_ names:

*@build*++
	_mandatory_: *yes*++
	_syntax_: *inline*++
	_valid value_:++
		*auto*: creates the service script file as *execline* script.++
			The corresponding file to start the service will automatically be++
			written in *execline* format with the *@execute* key _value_.++
		*custom*: creates the service script file in the language set in the++
			*@shebang* key _value_.++
			The corresponding file to start the service will be written++
			in the language set in the *@shebang* key _value_.

	Without equivalent, this key is unique to *66* tools.

*@runas*++
	_mandatory_: *no*++
	_syntax_: *inline*++
	_valid value_:++
		Any valid user set on the system.

	This will pass the privileges of the service to the given user before
	starting the last command of the service.
	*Note*: The service needs to be first started with root if you want to hand
	over priviliges to a user. Only root can pass on privileges. This field has
	*no effect* for other use cases.

	Without equivalent, this key is unique to *66* tools.

*@shebang*++
	_mandatory_: *yes* if the *@build* key is set to *custom*.++
	_syntax_: *quotes*, *path*++
	_valid value_:++
		A valid path to an interpreter installed on your system.

	This will set the language that will be used to read and write the
	*@execute* key _value_.

	Without equivalent, this key is unique to *66* tools.

*@execute*++
	_mandatory_: *yes*++
	_syntax_: *bracket*++
	_valid value_:++
		The command to execute when starting the service.

	*Note*: The field will be used as is. No changes will be applied at all.
	It's the responsability of the author to make sure that the content of
	this field is correct.

	Corresponds to the file *run* for a *classic* or *longrun* service 
	and to the file *up* for a *oneshot* service.

	Without equivalent, this key is unique to *66* tools.

## [stop]

This section is *optional*.

This section is exactly the same as *[start]* and shares the same keys. With
the exception that it will only be considered when creating the file *finish* for a *classic* or *longrun* service and when creating the file *down* for a *oneshot* service to create its content.

## [logger]

This section is *optional*.

The value *log* must be added to the *@options* key in the *[main]* section for
*[logger]* to have any effect.
This section extends the *@build*, *@runas*, *@shebang* and *@execute* key
fields from *[start]*/*[stop]* and the *@timeout-finish* and *@timeout-kill*
key fields from *[main]*. These are also valid keys for *[logger]* and behave
the same way they do in the other sections but they can not be specified except
for the *mandatory* key *@build*; see examples below. 
In such case the default behaviour for those key are apply.
Furthermore there are some keys specific to the log.

Valid _key_ names:++
*@build*, *@runas*, *@shebang*, *@execute* (see *[start]*)++
*@timeout-finish*, *@timeout-kill* (see *[main]*)

*@destination*++
	_mandatory_: *no*++
	_syntax_: *path*++
	_valid value_;++
		Any valid path on the system.

	The directory where to save the log file. This directory is automatically
	created. The current user of the process needs to have sufficient
	permissions on the destination directory to be able to create it.
	The default directory is *%%system_log%%/servicename* for *root* and
	*$HOME/%%user_log%%/log/servicename* for any regular user.
	can also be changed at compile-time by passing the
	*--with-system-logpath=*_DIR_ option for root and
	*--with-user-logpath=*_DIR_ for a user to *./configure*.

	Without equivalent, this key is unique to *66* tools.

*@backup*++
	_mandatory_: *no*++
	_syntax_: *uint*++
	_valid value_:++
		Any valid number.

	The log directory will keep _value_ files.
	The next log to be saved will replace the oldest file present.
	By default 3 files are kept.

	Without equivalent, this key is unique to *66* tools.

*@maxsize*++
	mandatory_: *no*++
	_syntax_: *uint*++
	_valid value_:++
		Any valid number.

	A new log file will be created every time the current one approaches
	_value_ bytes. By default, filesize is *1000000*; it cannot be set lower than *4096* or higher than *268435455*.

	Without equivalent, this key is unique to *66* tools.

*@timestamp*++
	mandatory_	: *no*++
	_syntax_: *inline*++
	_valid value_:++
		*tai*:++
			The logged line will be preceded by a TAI64N timestamp (and a++
			space) before being processed by the next action directive.++
		*iso*++
			The selected line will be preceded by a ISO 8601 timestamp for++
			combined date and time representing local time according to the++
			systems timezone, with a space (not a 'T') between the date and++
			the time and two spaces after the time, before being processed by++
			the next action directive.

	Without equivalent, this key is unique to *66* tools.

## [environment]

This section is *optional*.
It will only have an effect when the value *env* is added to the *@options* key
in the *[main]* section.
A file contained the *key=value* pair(s) will be created by default at
*%%service_admconf%%/name_of_service* directory. The default can also be
changed at compile-time by passing the *--with-sysadmin-service-conf=*_DIR_ option to
*./configure*.

Valid _key_ names:

Any *key=value* pair:++
	_mandatory_: *no*++
	_syntax_: *pair*++
	_valid value_:++
		You can define any variables that you want to add to the environment++
		of the service.

```
    [environment]
    run_dir=/run/openntpd
    cmd_args=-d -s
```

	The *!* character can be put in front the _value_ like this:

```
    [environment]
    rund_dir=!/run/openntpd
    cmd_args=!-d -s
```

	This will explicitly *not* set the _value_ of the _key_ for the runtime
	process but only at the start of the service. 
	In this example the _key_*=*_value_ pair passed to the command line does
	not need to be present in the general environment variable of the
	service. 

	Without equivalent, this key is unique to *66* tools.

## [regex]

This section is *optional*.
It will only have an effect when the service is a _module_ type.
You can use the '@I' string as key field. It will be replaced by the _module_
name as you do for instantiated service fefore applying the regex section.

Valid _key_ names:

*@configure*++
	_mandatory_: *no*++
	_syntax_: *quotes*++
	_valid value_:++
		You can define any arguments to pass to the module's configure script.

	Without equivalent, this key is unique to *66* tools.

*@directories*++
	_mandatory_: *no*++
	_syntax_: *pair* inside *bracket*++
	_valid value_:++
		Any _key=value_ pair where _key_ is the regex to search on the++
		directory name and _value_ the replacement of that regex. For example:

```
	@directories = ( DM=sddm TRACKER=consolekit )
```

	Where the module directory contains two sub-directories named use-DM
	and by-TRACKER directories. It will be renamed as use-sddm and by-consolekit
	respectively.

	Without equivalent, this key is unique to *66* tools.

*@files*++
	_mandatory_: *no*++
	_syntax_: *pair* inside *quotes*++
	_valid value_:++
		Reacts exactly as *@directories* field but on filename instead of++
		directories name.

	Without equivalent, this key is unique to *66* tools.

*@infiles*++
	_mandatory_: *no*++
	_syntax_: *colon*++
	_valid value_:++
		Any valid filename between the double colon with any _key=value_++
		where _key_ is the regex to search inside the file and _value_ the++
		replacement of that regex. The double colon *must* be present but++
		the name between it can be omitted. In that case, the _key=value_++
		pair will apply to all files contained on the module directories and++
		to all keys (regex) found inside the same file. For example:

```
    @infiles = ( :mount-tmp:args=-o noexec 
    ::user=@I )
```

	It replaces first the term @I by the name of the module.

	It opens the file named mount-tmp, search for the args regex and replaces
	it by the value of the regex.

	It opens all files found on the module directory and replaces all regex
	'user' found by the name of the module in each file.

# A word about the *@execute* key:

	As described above the *@execute* key can be written in any language as
	long as you define the key *@build* as *custom* and the *@shebang* key to
	the language interpreter to use. For example if you want to write your
	*@execute* field with bash:

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
	This is an unnecessary example but it shows how to construct this use case.
	The parser will put your *@shebang* at the beginning of the script and copy
	the contents of the *@execute* field. So, the resulting file will be:

```
    #!/usr/bin/bash
    echo "This script displays available services"
    for i in $(ls %%service_system%%); do
        echo "daemon : ${i} is available"
    done
```

	When using this sort of custom function *@runas* has *no effect*. You must
	define with care what you want to happen in a *custom* case.
	Furthermore when you set *@build* to *auto* the parser will take care about
	the redirection of the ouput of the service when the logger is activated.
	When setting *@build* to *custom* though the parser will not do this
	automatically. You need to explicitly tell it to:

```
    #!/usr/bin/bash
    exec 2>&1
    echo "This script redirects file descriptor 2 to the file descriptor 1"
    echo "Then the logger reads the file descriptor 1 and you have"
    echo "the error of the daemon written into the appropriate file"
```

	Moreover, for *oneshot* type the *@shebang* needs to contain the
	interpreter options as below:

```
    @build = custom
    @shebang = "/usr/bin/bash -c"
    @execute = ( echo "this is a oneshot service with a correct shebang definition" )
```

	Finally you need to take care about how you define your environment
	variable in the section *[environment]*. When setting *@build* to *auto*
	the parser will also take care about the *!* character if you use it.
	This character will have *no effect* in the case of *custom*. 

	This same behavior applies to the *[logger]* section.
	The fields *@destination*, *@backup*, *@maxsize* and *@timestamp* will have
	*no effect* in a *custom* case. You need to explicitly define the program to use the logger and the options for it in your *@execute* field.

# Instance service file creation

An _instance_ service file is of the same syntax as decribed in this document
for any other service. It can be any *type* of service. However some
differences exist:
	- the name of the file needs to be appended with an *@* (_commercial at_) character.
	- every value replaced in an instance file needs to be written with *@I*.

# EXAMPLE

## FILE NAME

```
%%service_system%%/dhcpcd
%%service_system%%/very_long_name
```

## FILE CONTENT

```
[main]
@type = classic
@name = ntpd
@description = "ntpd daemon"
@user = ( root )
@options = ( log env )

[start]
@build = auto
@execute = ( foreground { mkdir -p  -m 0755 ${dir_run} } 
execl-cmdline -s { ntpd ${cmd_args} } )

[environment]
dir_run=!/run/openntpd
cmd_args=!-d -s
```

## tty@

File name: tty@
Contents:
```
[main]
@type = classic
@description = "Launch @I"
@user = ( root )

[start]
@build = auto
@execute = ( agetty -J 38400 @I } )
```

	By using *66-enable tty@tty1*, the resulting file will then be:
```
[main]
@type = classic
@description = "Launch tty1"
@user = ( root )

[start]
@build = auto
@execute = ( agetty -J 38400 tty1 } )
```
# MODULE SERVICE CREATION

A module can be considered as an instantiated service. It works as the 
same way as a service frontend file but allows to configure a set of 
different kind of services before executing the enable process. Also, 
the set of the services can be configured with the conjunction of a 
script called _configure_ which it can be made on any language.

A module is defined with two elements: an instantiated frontend service
file at %%service_system%% and a directory at %%service_module%%. 
The name of the frontend and the name of the directory *must* be the same. 
For example if the frontend is named foo@, the directory of the module
must be foo@.

The module directory can contain a sub-directory named _.configure_
with an *executable* file script named _configure_ inside. For example,
foo@/.configure/configure. The sub-directory *must* be named _.configure_
and the file script *must* be named _configure_.

It's up to you to write the _configure_ script file with the language 
of your choice as long as you define a correct _shebang_.

The _configure_ script is launched after the parse of the frontend file 
meaning all regex on directories and files are already made.

## A word about the *[main]* section with the module type.

The valid field in section *[main]* are:++
	@type++
	@description++
	@name++
	@version++
	@user++
	@depends++
	@optsdepends++
	@extdepends++
	@hiercopy

All other fields from *[main]* section are not allowed.

## PROTOTYPE

This prototype contain all valid *section* with all valid _key_*=*_value_ pair.
```
[main]
@type = classic,bundle,longrun,oneshot
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
MYKEY=!myvalue
ANOTHERKEY=!antohervalue

[regex]
@configure="arguments to pass to configure script"
@directories=(key=value key=value)
@files=(key=value key=value)
@infiles=(:filename:key=value ::key=value)
```
