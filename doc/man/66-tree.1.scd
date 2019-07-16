66-tree(1)

# NAME

66-tree - Handle a directory containing a set of services.

# SYNOPSYS

66-tree [ *-h* ] [ *-v* _verbosity_ ] [ -l ] [ *-n* | *-R* ] [ *-a* | *-d* ] [ *-c* _clone_ ] [ *-E* | *-D* ] [ *-U* ] [ *-C* ] _tree_

# DESCRIPTION

*66-tree* will dynamically handle a set of any kind of services in _tree_.
The _tree_ will be handled for the user running the process.
Any _tree_ is completely independent from another. If you want to know what trees
are currently available on the system use *66-info*(1) tool.

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.
	*1* : (Default) Only print error messages.
	*2* : Also print warning messages.
	*3* : Also print debugging messages.

*-l* _live_
	An absolute path. It used in conjunction with *-U* and *-R* options 
	to specify the live	directory place of _tree_. It has no effects otherwise.
	Default is *%%livedir%%*. The default can also be changed at compile-time
	by passing the \--livedir=_live_ option to *./configure*. Should be under
	a writable filesystem - likely a RAM filesystem.
	
*-n*
	Creates a new empty _tree_. The new _tree_ must not exist on the system.

*-a* _user_
	Allows _user_ to deal with the given _tree_. Must match an existing username
	on the system. Several users can be allowed by separating their names with a
	comma.
	Any _user_ not explicitly allowed is automatically denied for the given
	_tree_.
	By default the user issuing the command is automatically allowed when the
	_tree_ is created. This option sets the _UID_ and _GID_ of the service
	database at compilation time.

*-d* _user_
	Denies _user_ to deal with the given _tree_. Must match an existing username
	on the system. Several users can be denied by separating their names with a
	comma. Useful to revoke access that has been given before with *-a*.

*-c*
	Makes _tree_ the current tree to use by default with other 66 tools.
	The majority of commands in the 66 tools suite have a *-t* option to specify
	the _tree_ to use.
	By setting _tree_ as the current one all other 66 tools will use it by
	default. You can see which _tree_ is the current one with the *66-info*(1) tool.

*-E*
	Enables _tree_. This allows the *66-all*(1) tool to know which tree needs to
	be started. If the given _tree_ is enabled, all services in that tree will be
	started when you use the *66-all*(1) tool.

*-D*
	Disables _tree_. The strict opposite of the *-E* option.

*-R*
	Deletes _tree_. Can not be *undone*. This will completely remove the given
	tree from the system! You will not be able to retrieve any information of the
	deleted _tree_ after deleting it. Services currently running on _tree_ will
	be *not* bringed down before remove it. To do so, use the *-U* option in
	conjonction e.g. "66-tree -UR _tree_".

*-U*
	Unsupervise _tree_. Bring down all services contained into the _tree_ and
	remove the corresponding directory of the service from the scandir.

*-C* _clone_
	Makes a strict copy of _tree_ named _clone_. Useful to create a backup of
	_tree_ or to start a new tree from an existing one. _clone_ must not exist
	on the system.

# WHY TREES?

The usefulness of having several trees with different services can be explained with a simplified example.

```
    Tree1 contains dhcpcd & ntpd
    Tree2 contains cups & nfs
    Tree3 contains xorg, notification-daemon, gvfsd & dbus
```

When you boot your machine and want to use it from console only, you don't care about xorg or cups, you only care about a working internet connection. So at the base you only have Tree1 enabled. At every boot this tree and all its services will now be automatically started.
Then you need to print something but for this, you also need to start the nfs daemon because your document is on some other server. Normally you would need to start cups then start nfs. Using the concept of trees you start Tree2 and everything is available. When you have finished printing your awesome document instead of stopping the needed services one by one you simply stop the whole Tree2 and all its containing services are stopped automatically.
This doesn't stop here. Now you want to see a video, you need a running X server and probably several other services. Tree3 was designed just for that.

# DIRECTORIES AND FILES

*Caution:* avoid manipulating manually any directories and sub directories and
their containing files of a _tree_. Corruption and errors may occur by doing so.
The following details can help better understand the mechanics of the 66 tools.
Nevertheless the 66 suite manages these paths dynamically and its tools like
*66-enable*(1), *66-disable*(1), *66-start*(1), *66-dbctl*(1), etc. will
automatically search these directories to find information about a required service.

## AT CREATION TIME

When creating a new tree, by default it will be created at
%%system_dir%%/system/_tree_ for root and at $HOME/%%user_dir%%/system/_tree_
for any regular user.

## WHEN ENABLING A SERVICE

By enabling a service its corresponding frontend service file is parsed and the
result written to a sub directory of the trees path called *servicedirs/svc* for
_classic_ services and *servicedirs/db/source* for _bundle_ or _atomic_ services.

As an example for root the resulting files would, by default, be found at
%%system_dir%%/system/_tree_/servicedirs/svc for services of type _classic_
and at %%system_dir%%/system/_tree_/servicedirs/db/source for any service of type
_bundle_ or _atomic_.
	
## WHEN ENABLING A WHOLE TREE

When you ask to start all services of the currently enabled <em>tree</em> at once
with the *66-all*(1) tool, the directory
%%system_dir%%/system/_tree_/servicedirs/svc is opened and the command will start
any service found inside of that directory.

For services of type _bundle_ and _atomic_, instead of opening the directory
%%system_dir%%/system/_tree_/servicedirs/db/source, the corresponding compiled
database found at %%system_dir%%/system/_tree_/servicedirs/db/_tree_ is used.
The database found at this location is the result of an automatic use of the
command *s6-rc-compile* when enabling such a service.
