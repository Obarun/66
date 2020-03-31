66-disable(1)

# NAME

66-disable - Disables one ore more services inside a given _tree_.

# SYNOPSIS

66-disable [ *-h* ] [ *-z* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ *-t* _tree_ ] [ *-S* ] _service_...

# DESCRIPTION

*66-disable* expects to find an already enabled service inside the given _tree_.

The targeted _service(s)_ can also be stopped on the fly when disabling it with
the *-S* option.

Generally speaking this command is the strict opposite of the *66-enable*(1)
tool.

Multiple _services_ can be disabled by seperating their names with a space.
In case of _bundle_ or _atomic_ services, any dependency chain will be
automatically resolved. It is unnecessary to manually define chained sets
of dependencies. If FooA has a declared dependency on FooB, FooB will be
automatically disabled as well when disabling FooA. This will run
recursively until all dependencies are disabled.

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
	Changes the supervision directory of _service_ to _live_. By default
	this will be *%%livedir%%*. The default can also be changed at compile time
	by passing the *--livedir=*_live_ option to *./configure*. An existing
	absolute path is expected and should be within a writable filesystem -
	likely a RAM filesystem. See *66-scandir*(1).

*-t* _tree_
	Handles the _selection_ of the given _tree_. This option is mandatory
	except if a tree was marked as 'current'. See *66-tree*(1).

*-S*
	Stops the _service_ on the fly directly after disabling it. If the
	state of the _service_ is already down, this option will have no effect.
