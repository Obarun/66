66-scandir(1)

# NAME

66-update - Handle a format transition update

# SYNOPSYS

66-update [ *-h* ] [ *-v* _verbosity_ ] [* -c* ] [ *-l* _live_ ] [ *-d* ]  _tree_

# DESCRIPTION

*66-update* is a program that should be used  rarely. At upgrade/downgrade 
time, the inner file format used to retrieve informations of tree,service 
database currently in use, can differ between two versions. In this case, 
service databases and trees need to be rebuild to match the new format. 
The 66-update program makes a complete transition of trees and the live 
directory using a old 66 format (the one being replaced) with the new 66 format.

The goal is to ensure a smooth transition between different versions of 66 
without needing a reboot. 

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	*1* : (Default) Only prints error messages.++
	*2* : Also prints warning messages.++
	*3* : Also prints tracing messages.++
	*4* : Also prints debugging messages.

*-c*
	enable colorization

*-l* _live_
	An absolute path. Create the scandir at _live_. Default is *%%livedir%%*.
	The default can also be changed at compile-time by passing the
	\--livedir=_live_ option to *./configure*. Should be under a writable
	filesystem - likely a RAM filesystem.

*-d*
	dry run. Performs operation without modify the current state of the 
	system. All command's output will be prefixed by "dry run do: " 
	sentence. It's a good idea to first use this option before performing 
	a real update.

If no _tree_ is given, all trees of the owner (root or user) of the 
process will be processed. In the case of user owned trees *66-update*
must run separately for each user.

# TRANSITION PROCESS

It determines if the tree is already initialized. In such case, it
makes a copy of the current database on a temporary directory and
switches the database to it.

It saves the current state of the tree (current,enabled,...).

It destroys the tree with *66-tree -R* command. 

It creates the tree with *66-tree -n(cEa)* command, reproducing the options
of the saved state.

It enables the service(s) previously declared on the _tree_.

It switches the current database to the newly created database by the enable process.

At the end of the process, the tree and the live state has not changed. Services already is use
are not stopped or restarted during the transition process and match exactly the same state
before and after the process.

It tries to be safe as possible and exit 111 is a problem occurs. In any case 
on upgrade/downgrade time, 66 tools will not work properly if the transition 
is not made smoothly even by the sysadmin itself. So, we prefer to crash instead of
leaving an inconsistent state and let the sysadmin correct the things.
