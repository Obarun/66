66-scandir(1)

# NAME

66-scandir - Handle the scandir for a given user

# SYNOPSYS

66-scandir [ *-h* ] [ *-v* _verbosity_ ] [ *-b* ] [ *-l* _live_ ] [* -t* _rescan_ ] [ *-L* _log_user_ ] [ *-s* _skel_ ] [ *-e* _environment_ ] [ *-c* | *-r* | *-u* ] _owner_

# DESCRIPTION

*66-scandir* creates or start the scandir (a.k.a. directory containing a
collection of *s6-supervise* processes) for the given _owner_ depending on the
provided options. Note that _owner_ can be any valid user on the system. However, the given user must have sufficient permissions
to create the necessary directories at its location. That is *%%livedir%%* by
default or the resulting path provided by the *-l* option. If _owner_ is not
explicitely set then the user of the current process will be used instead.

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	*1* : (Default) Only print error messages.++
	*2* : Also print warning messages.++
	*3* : Also print debugging messages.

*-b*
	Create specific files for boot. Only root user can use this option. It is
	not meant to be used directly even with root. *66-boot*(8) calls it during
	the boot process.

*-l* _live_
	An absolute path. Create the scandir at _live_. Default is *%%livedir%%*.
	The default can also be changed at compile-time by passing the
	\--livedir=_live_ option to *./configure*. Should be under a writable
	filesystem - likely a RAM filesystem.

*-d* _notif_
	notify readiness on file descriptor _notif_. When scandir is ready to 
	accept commands from *66-scanctl*, it will write a newline to _notif_.
	_notif_ cannot be lesser than 3. By default, no notification is sent.
	if *-b* is set, this option have no effects.

*-t* _rescan_
	Perform a scan every _rescan_ milliseconds. If _rescan_ is set to *0*
	(the default), automatic scans are never performed after the first
	one and *s6-svscan* will only detect new services by  issuing either
	"66-scandir -s reload" or "s6-svscanctl -a". It is *strongly*
	discouraged to set _rescan_ to a positive value under 500.

*-s* _skel_
	An absolute path. Directory  containing _skeleton_ files. This option is
	not meant to be used directly even with root. *66-boot*(8) program calls it
	at during the boot process.

*-L* _log_user_
	Will run as the catch-all logger as _log_user_ user. Default is root.

*-e* _environment_
	An absolute path. Merge the current environment variables of _scandir_
	with variables found in the directory. Any file in _environment_
	not beginning with a dot and not containing the '=' character will
	be read.

*-c*
	Create the neccessary directories at _live_.
	Must be executed once with this option before being able to use *-u*.

*-r*
	Remove the scandir directory at _live_.
	If it was already started with *-u* it must first be stopped sending
	a signal with "66-scanctl quit" or similar.

*-u*
	Start the _scandir_ directory at _live_ invocating the *s6-svscan* program.

# SCANDIR CREATION PROCESS

When creating the _scandir_ with the *-c* option various files and directories
will be created at the _live_ directory for the given _owner_.

If created with the *root* user, you will find the following in *%%livedir%%*
(the directory created by default if *-l* is not passed and *0* being the
corresponding UID for the root user):

*%%livedir%%/scandir/0* : stores all longrun proccesses (commonly known as
	daemons) started by the user *root*.
*%%livedir%%/tree/0* : stores any s6-rc service database started by *root*.
*%%livedir%%/log/0* : stores the catch-all logger when the scandir is created
	for a boot procedure with the *-b* option.
*%%livedir%%/state/0* : stores internal 66 ecosystem files.

If created with a regular user you will find the following in *%%livedir%%*
(Default directories if *-l* is not passed and *1000* being the UID for the
user):

*%%livedir%%/scandir/1000*

*%%livedir%%/tree/1000*

*%%livedir%%/state/1000*

If a _scandir_ already exists for the given user it will prevent
the creation when issuing "66-scandir -c". If you want to create a different
_scandir_ for the same _owner_ you must delete it first with *-r*.

# ENVIRONMENT CONFIGURATION

You can modify environment variables when starting the _scandir_ with the *-e*
option. This option expects to find a valid absolute path for a directory
containing one or more files where the format is the classic _key=value_ pair.
Each file found will be read and parsed. See *execl-envfile*(5) for more.

Any longrun process launched on the scandir should inherit the environment
variables of the scandir. A specific global _key=value_ pair inherited by all
daemons can be set using this option.

# SCANDIR REMOVE PROCESS

When using the *-r* option to delete the scandir of a given user some
directories of the scandir may not be removed if another user accesses them. In
our previous example where we created a scandir for root with UID *0* and a
regular user with the UID *1000* this would imply the following:

*%%livedir%%/scandir/0*		# will be deleted++
*%%livedir%%/tree/0*			# will be deleted++
*%%livedir%%/log/0*			# will be deleted++
*%%livedir%%/state/0*			# will be deleted

The _live_ directory of the *root* user (by default *%%livedir%%* is used) will
not be removed because another user (the one from our example) can still use
it. In fact *66-scandir* will only remove the subdirectories of the
corresponding UID of the _owner_ while the _live_ root directory is not
touched. If _live_ was created on a RAM filesystem the deletion happens on the
next reboot.

*Note*: A running scandir can not be removed. It needs to be stopped first with
	66-scanctl quit
or similar to be able to remove it.

# BOOT SPECIFICATION

The *-b* and *-s* options are called by *66-boot*(8) program. *-b* option
will create .s6-svscan control files (see *s6-svscan* interface documentation)
specifically for the _stage1_ a.k.a PID1 process. This special _scandir_ is
controlled by the safe wrappers *halt*, *poweroff*, *reboot*, *shutdown*
wrapper provided with *66* tools. The *66-shutdownd*(8) daemon which control the shutdown request will be created automatically at the correct location.
Further this specific tasks need to read of the _skeloton_ file _init.conf_
containing. the _live_ directory location which is the purpose of the *-s*
option.

The _live_ directory for a boot process requires writable directories, so in
order to accommodate read-only root filesystems, there needs to be a tmpfs
mounted before *s6-svscan* program can be run.
