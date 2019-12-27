66-scandir(1)

# NAME

66-update - Handle a format transition update

# SYNOPSYS

66-scandir [ *-h* ] [ *-v* _verbosity_ ] [* -c* ] [ *-l* _live_ ]  _tree_

# DESCRIPTION

*66-update* is a program that is normally used very rarely: when the 
66 package is upgraded or downgraded, some services are currently managed, 
different tree as declared and the inner format has changed. This program transitions
a complete tree and a live directory using a database with the old 66 format (the one being replaced) 
so that it uses a database with the new 66 format and rebuild the tree.

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	*1* : (Default) Only print error messages.++
	*2* : Also print warning messages.++
	*3* : Also print tracing messages.++
	*4* : Also print debugging messages.

*-c*
	enable colorization

*-l* _live_
	An absolute path. Create the scandir at _live_. Default is *%%livedir%%*.
	The default can also be changed at compile-time by passing the
	\--livedir=_live_ option to *./configure*. Should be under a writable
	filesystem - likely a RAM filesystem.

If no _tree_ is given, all trees of the owner of the process will be processed.

# TRANSITION PROCESS

It determines if the tree is already initialized. In such case, it
make a copy of the current database on a temporary directory and
switch the database to it.

It saves the current state of the tree (current,enabled,...).

It destroys the tree with *66-tree -R* command. 

It creates the tree with *66-tree -n(cEa)* command, options depending of the saved state.

It enables the service(s) previously declared on the _tree_.

It switch the current database to the newly created by the enable process.

At the end of the process, the tree and the live state has not changed. Services already is use
are not stopped or restarted during the transition process and match exactly the same state
before and after the process.

It try to be safe as possible and exit 111 is a problem occurs. In any case 
on upgrade/downgrade time, 66 tools will not work properly if the transition 
is not made smoothly even by the sysadmin itself. So, We prefer to cratch instead of
leaving an inconsistent state and let the sysadmin correct the things.
