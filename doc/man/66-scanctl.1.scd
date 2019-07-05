66-scanctl(1)

# NAME

66-scanctl - Send signal to a _scandir_

# SYNOPSIS

66-scanctl [ *-h* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ *-o* _owner_ ] _signal_

# DESCRIPTION

This is a safe wrapper around *s6-svscanctl*.

66-scandir sends _signal_ to an already running _scandir_ at _live_ where by
default _live_ is *%%livedir%%* by default or the resulting path provided by
the *-l* option. If _owner_ is not explicitely set with *-o* then the user of
the current process will be used instead.

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.
	- *1* : (Default) Only print error messages.
	- *2* : Also print warning messages.
	- *3* : Also print debugging messages.

*-l* _live_
	An absolute path. Send the signal to a _scandir_ at _live_. By default is
	%%livedir%%. The default can also be changed at compile time by
	passing the *--livedir=*_live_ option to *./configure*. Should be within a
	writable filesystem - likely a RAM filesystem. See *66-scandir*(1).

*-o* _owner_
	Send the signal to a _scandir_ owned by _owner_ instead of the current
	owner of the process. _owner_ should have the correct permissions access to
	deal with the _scandir_.

# SIGNALS

Any signal accepted by *s6-svscanctl* can be passed just without the dash *-*
character. As a result if you wanted to send a *-t* signal, you need to use:
	66-scanctl t
Further a series of commands is also accepted in the same way:
	66-scanctl st
A few convenient keywords were added to avoid having to remember basic and
useful commands:

	- *reload*: equal to *-an*
	- *interrupt*: equal to *-i*
	- *quit*: equal to *-q*
	- *halt*: equal to *-0*
	- *reboot*: equal to *-6*
	- *poweroff*: equal to *-7*

# EXAMPLES

66-scanctl reload

Updates the process supervision tree to exactly match the services listed in
_scandir_. This command is strictly equal to :

s6-svscanctl -an /service
