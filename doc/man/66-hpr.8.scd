66-hpr(8)

# NAME

66-hpr - Trigger the software shutdown procedure

# SYNOPSIS

66-hpr [ *-H* ] [ *-l* _live_ ] [ *-b* _banner_ ] [ *-f* ] [ *-h* | *-p* | *-r* ] [ *-d* | *-w* ] [ *-W* ]

# DESCRIPTION

*66-hpr* triggers the software shutdown procedure. Performs an immediate
hardware shutdown with the *-f* option.

It is normally invoked through *halt*, *poweroff* or *reboot* wrappers
installed by default at *%%skel%%*.

This program is a modified copy of *s6-linux-init-hpr* program.

- If the *-f* option is passed the system is stopped or rebooted immediately
  without properly cleaning up.
- Else, the machine's shutdown procedure is started.
- The command exits 0; the shutdown procedure happens asynchronously.

This is the traditional _sysvinit_ interface for the *halt*, *poweroff* and
*reboot* programs.

*66-hpr* must always be called with either *-h*, *-p* or *-r* options.

# OPTIONS

*-H*
	Prints this help.

*-l* _live_
	An absolute path. The directory to use. *66-hpr* sends signal to the
	listening pipe of the *66-shutdownd*(8) service at
	_live_/scandir/0/shutdownd/fifo. Default is *%%livedir%%*. The default can
	also be changed at compile-time by passing the --livedir=_live_ option to
	*./configure*. An absolute path is expected and should be under a writable
	filesystem - likely a RAM filesystem.

*-b*
	_banner_ . Text to display before executing the shutdown process. Default
	to:
```
*** WARNING *** 
The system is going down NOW!
```

*-f*
	Force. The command will not trigger a clean shutdown procedure; it will
	only sync the filesystems then tell the kernel to immediately halt,
	poweroff or reboot. This should be the last step in the lifetime of the
	machine.

*-h*
	Halt. The system will be shut down, but the power will remain connected.

*-p*
	Poweroff. Like halt but the power will also be turned off.

*-r*
	Reboot. The system will initialize a warm boot without disconnecting power.

*-d*
	Do not write a wtmp shutdown entry. See *utmp*(5).

*-w*
	Only write a wtmp shutdown entry; do not actually shut down the system.

*-W*
	Do not send a _wall_ message to users before shutting down the system. Some
	other implementations of the *halt*, *poweroff* and *reboot* commands use
	the *--no-wall* long option to achieve this.

# NOTES

*halt*, *poweroff* and *reboot* scripts that call *66-hpr* with the relevant
option *should be copied or symlinked* by the administrator into the binary directory of your system.
