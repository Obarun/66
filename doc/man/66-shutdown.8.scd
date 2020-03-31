66-shutdown(8)

# NAME

66-shutdown - Trigger the system shutdown procedure

# SYNOPSIS

66-shutdown [ *-H* ] [ *-l* _live_ ] [ *-h* | *-p* | *-r* | *-k* ] [ *-a* ] [ *-t* _sec_ ] [ *-f* | *-F* ] _time_ [ _message_ ]

66-shutdown *-c* [ _message_ ]

# DESCRIPTION

It is normally invoked as *shutdown* at your binary system directory. This
program is a modified copy of *s6-linux-init-shudown* program.

- Will cancel a pending shutdown if *-c* is passed.
- Else, plans the shutdown procedure at time _time_.
- If a _message_ argument has been given, it is broadcasted to all logged
  in users (as tracked by utmp).
- Exits with *0*. The shutdown procedure happens asynchronously.

The *66-shutdown* program abides to the standards of the *LSB-3.0.0 shutdown*
interface.

_time_ must follow the following format: [ *now* | [+]_mins_ | _hh_:_mm_ ]

	- *now* means: triggers the shutdown sequence immediately.
	- _mins_ or *+*_mins_ means: relative time. Triggers the shutdown sequence
	  after _mins_ minutes.
	- _hh_:_mm_ means: absolute time. Triggers the shutdown sequence when the
	  time _hh_:_mm_ occurs. If that time has passed for the current day, it
	  will wait for the next day. _hh_ must have 1 or 2 digits; _mm_ must have
	  2 digits.

# OPTIONS

*-H*
	Prints this help.

*-l* _live_
	An absolute path. Directory to use. *66-shutdown* sends signal the
	listening pipe of the *66-shutdownd*(8) service at
	_live_/scandir/0/shutdownd/fifo. Default is *%%livedir%%*. The default can
	also be changed at compile-time by passing the --livedir=_live_ option to
	*./configure*. An absolute path is expected and should be under a writable
	filesystem - likely a RAM filesystem.

*-a*
	Access control. The shutdown sequence will only be launched if one of the
	users listed in *%%skel%%/shutdown.allow* is currently logged in (as tracked by
	utmp). *%%skel%%/shutdown.allow* is a text file which accepts one user per
	line. Lines starting with *#* are comments.

*-t* _sec_
	Have a "grace time" period of _sec_ seconds between the SIGTERM and the
	SIGKILL  at the end of the shutdown sequence when it is time to kill all
	processes (to allow processes receiving SIGTERM to exit cleanly). Default
	is 3 seconds.

*-k*
	Do not shut down. Send a warning _message_ to all logged in users.

*-h*
	Halt the system at the end of the shutdown sequence.

*-p*
	Power off the system at the end of the shutdown sequence. This option is
	provided as an extension, it is not required by the LSB interface.

*-r*
	Reboot the system at the end of the shutdown sequence.

*-f*
	Ignored.

*-F*
	Ignored.

*-c*
	Cancel a planned shutdown Can only cancel the effect of a previous call to
	*shutdown* with a _time_ argument that was not _now_. This cannot be used
	to interrupt a shutdown sequence that has already started.

# NOTES

- The *66-shutdown* binary is not meant to be called directly. It shuold
  always be referenced by the *shutdown* _skeloton_ file installed by default
  at *%%skel%%* directory. The binary *should be copied or symlinked* by
  the administrator into the binary directory of your system.

- *-f* and *-F* options are only accepted for compatibility. LSB says they are
  used to advise the system to skip or enforce a *fsck* after rebooting. But
  they are only advisory, and for decades now systems have used other methods
  of evaluating whether they should perform filesystem checks, so these options
  are largely obsolete nowadays.
