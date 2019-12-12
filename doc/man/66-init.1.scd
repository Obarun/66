66-init(1)

# NAME
66-init - Initiate services from a given tree at a live directory

# SYNOPSYS

66-init [ *-h* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ *-t* _tree_ ] *classic* | *database* | *both*

# DESCRIPTION

*66-init* expects to find an already created _scandir_ (see *66-scandir*(1))
directory at _live_ location. The _scandir_ does not need to be necessarily
running depending on the provided options.

Administrators should invoke *66-init* only once.

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.
	- *1* : (Default) Only print error messages.
	- *2* : Also print warning messages. 
	- *3* : Also print tracing messages.
	- *4* : Also print debugging messages.

*-l* _live_
	Changes the _scandir_ directory to _live_. By default this will be
	*%%livedir%%*. The default can also be changed at compile time by passing
	the --livedir=_live_ option to *./configure*. An existing absolute path is
	expected and should be within a writable filesystem - likely a RAM
	filesystem. See *66-scandir*(1).

# ARGUMENTS

*classic*
	Only initiate all _classic_ services from _tree_
	at _live_ location.

*database*
	Only initiate all _bundle_ and _atomic_ services from _tree_
	at _live_ location.

*both*
	Initiate all _classic_ services and all _bundle_ and _atomic_ services
	from _tree_ at _live_ location.

# INITIATLIZATION PROCESS

## CLASSIC SERVICES

*66-init* will make an exact copy of all classic service files and directories
of the _tree_ inside a _scandir_ directory at _live_.
By default *%%livedir%%scandir/UID* is created if it does not exist yet, where
*UID* is the uid of the current owner of the process. The _scandir_ does *not*
need to be necessarily running. This is useful at boot time to initiate an
early service before starting the _scandir_. Once the _scandir_ 
starts; see *66-scandir*(1) -u option, the already present services start
automatically.
If the _scandir_ is running, you should invoke a *66-scanctl reload* command
to inform it about the changes.

## BUNDLE, ATOMIC SERVICES

The tool will automatically invoke

```
s6-rc-init -l live -c compiled -p prefix,
```

where, by default, _live_ translates to *%%livedir%%/tree/UID/<tree>* and
_compiled_ to *%%system_dir%%/system/<tree>/servicedirs/db/<tree>* or
*$HOME/%%user_dir%%/system/<tree>/servicedirs/db/<tree>* depending on
the owner of the process and the _prefix_ of the name of the _tree_.
If it doesn't exist yet _live_ is created in the process. For these
services the _scandir_ *must* be running.
This tool like any other 66 tool can be invoked with user permissions.

# SEE ALSO

*66-scandir*(1)
