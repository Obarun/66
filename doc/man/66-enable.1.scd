66-enable(1)

# NAME

66-enable - Enable one ore more services inside a given _tree_.

# SYNOPSIS

66-enable [ *-h* ] [ *-z* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ *-t* _tree_ ] [ *-f|F* ] [ *-c|*m*|C* ] [ *-S* ] _service_...

# DESCRIPTION

*66-enable* expects to find a corresponding _frontend service file_ (see
*66-frontend*(5)), a _directory name_ or a  _service instance_, by default at
*%%service_adm%%* or *%%service_system%%* in this order of precedence for
root user and *%%service_user%%*, *%%service_adm%%* or *%%service_system%%*
in this order of precedence for a normal user. The default path can be changed
at compile time by passing the --with-sys-service=_DIR_, \--with-packager-service=_DIR_ and --with-user-service=_DIR_ to
*./configure.*

It will run a parser on the frontend service file and write the result to
the directory of the given _tree_ (see *66-tree*(1)). The _service_ will
then be available in the given _tree_ for the next boot depending on the
state of the _tree_. The targeted _service(s)_ can also be started on the
fly when enabling it with the *-S* option.

Multiple _services_ can be enabled by seperating their names with a space.

# OPTIONS

*-h*
	Prints this help.

*-z*
	use color.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	- *1* : (Default) Only print error messages.++
	- *2* : Also print warning messages.++
	- *3* : Also print tracing messages.++
	- *4* : Also print debugging messages.

*-l* _live_
	Changes the supervision directory of _service_ to _live_. By default this
	will be %%livedir%%. The default can also be changed at compile time by
	passing the *--livedir=*_live_ option to *./configure*. An existing
	absolute path is expected and should be within a writable filesystem -
	likely a RAM filesystem. See *66-scandir*(1).

*-t* _tree_
	Specifies the _tree_ used to store the parsed _service_ file. This option
	is mandatory except if a tree was marked as 'current'. See *66-tree*(1).

*-f*
	Reenables an already enabled _service_ with the given options. This option
	will run again the process from the start and overwrite all existing files.

*-F*
	Same as *-f* but also reenables it dependencies.
	
*-c*
	only appends new _key=value_ pairs at environment configuration file from _frontend_ file.

*-m*
	appends new _key=value_ and merge existing one at environment configuration file from _frontend_ file.

*-C*
	overwrite it environment configuration file from _frontend_ file.

*-S*
	Starts the _service_ on the fly directly after enabling it. If the state of
	the _service_ is already up, this option will have no effect unless the
	*-f* option is used to reload it.


# DEPENDENCY HANDLING

For _services_ of type _bundle_ or _atomic_ any existing dependency chain will
be automatically resolved. It is unnecessary to manually define chained sets
of dependencies. If FooA has a declared dependency on another service with
the name FooB then FooB will be automatically enabled too when enabling FooA.
This works recursively until all necessary dependencies are enabled.

# DIRECTORY NAME AS SERVICE

When choosing to make a directory be recognised as service the path of the
directory must exist by default at *%%service_adm%%*, *%%service_system%%*
or *$HOME/%%service_user%%* depending of the owner of the process.
All _service_ files found in this directory will be enabled. This process
is done recursively if a sub-directory is found till it not found other
directories into the sub one. The directory can contain a mixed set of 
_bundle_ and _atomic_ services where some of those depend on each other. 
The directory option is not limited to these types though. Any available 
service type can be part of the set.

A good example is a set of services for the boot process. To achieve 
this specific task a large number of _oneshot_ services is used along
with some _classic_ services.

The parser automatically resolves any existing dependency chain for the
processed _services_ just as it would for any regular service.

This option and its mechanics can be subject to change in future releases
of the *66-enable* tool.

# INSTANCED SERVICE

An instanced _service_ name from a service template can be passed as argument
where the name of the _service_ must end with a *@* (commercial at) (see
*66-frontend*(5)).

The name of the template must be declared first immediately followed by the
instance name as shown in the following example:

66-enable tty@tty1

Also an instanced _service_ can be declared on the *@depends* field of the
frontend service file.

## SERVICE CONFIGURATION FILE

If the *[environment]* section is set on the frontend service file, the parse
result process can be found by default at *%%service_admconf%%* for the root
user and *$HOME/%%service_userconf%%* for a normal user. The default path can
be changed at compile time by passing the --with-sys-service-conf=_DIR_ for the
root user and --with-user-service-conf=_DIR_ for a normal user.

# SEE ALSO

*66-frontend*(5) *66-scandir*(1) *66-tree*(1)
