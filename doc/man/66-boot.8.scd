66-boot(8)

# NAME
66-boot - Run as pid 1 and prepare the system for scandir

# SYNOPSIS

66-boot [ *-h* ] [ *-s* _skel_ ] [ *-m* ] [ *-l* _log_user_ ] [ *-e* _environment_ ] [ *-d* _dev_ ] [ *-b* _banner_ ]

# DESCRIPTION

66-boot runs as pid 1, as a _stage1_ init; it performs the necessary early system preparation spawns a process that will run the _rc.init_ script, then execs into *66-scandir*(1).

# OPTIONS 

*-h*
	Prints this help.

*-s* _skel_
	Path of the skeleton files. By default this will be *%%sysconfdir%%*.
	The default can also be changed at compile time by passing the
	*--sysconfdir=*_DIR_ option to *./configure*. This directory *must*
	contains the necessaries skeleton files to properly boot the machine, without it the system *will not boot*. _skel_ must be an absolute path.

*-m*
	Umount the basename of the _LIVE_ directory set into the _init.conf_
	skeleton file and mount a tmpfs on it. This is the default.

*-l* _log_user_
	The catch-all logger will run as the _log_user_ user. Default is *root*.

*-e* _environment_
	_stage 1_ init empties its environment before spawning _rc.init_ skeleton
	file and executing into *66-scandir*(1), in order to prevent those "kernel"
	environment variables from leaking into the whole process tree. The *PATH*
	variable will be the only one variable set into the environment. If you
	want to define environment variable use this options. _environment_ must be
	an absolute path and reacts at the same as *66-scandir*(1) *-e* option.

*-d* _dev_
	Mount a devtmpfs on */dev*. By default, no such mount is performed - it is
	assumed that a devtmpfs is automounted on */dev* at boot time by the kernel
	or an initramfs.

*-b* _banner_
	Prints _banner_ to */dev/console* at the start of the _stage 1_ init
	process. By default "[Starts stage1 process ...]".

# EARLY PREPARATION

When booting a system, *66-boot* performs the following operations:

- It prints a banner to */dev/console*.
- It parse the _init.conf_ skeleton file.
- It chdirs to */*.
- It sets the umask to _initial_umask_.
- It becomes a session leader.
- It mounts a devtmpfs on */dev*, if requested.
- It uses */dev/null* as its stdin (instead of */dev/console*).
	- *dev/console* is still used, for now, as stdout and stderr.
- It check the _LIVE_ basename as valid mountpoint and unmount it in such
  case then mount a tmpfs on it if requested. If _LIVE_ basename is not
  valid mountpoint it mount directly a tmpfs on it.
- It create _LIVE_ directory invocating
  "66-scandir -v _VERBOSITY_ -l _LIVE_ -b -c -s _skel_" more "-L user_log" if
  requested.
- It initiate the early services of _TREE_ invocating
  "66-init -v VERBOSITY -l LIVE -t TREE classic".
- It reads the initial environment from _environment_ if requested.
- It performs "the fifo trick", i.e. it redirects its stdout to the catch-all
  logger's fifo, without blocking, before the catch-all logger is even up
  (because it's a service that will be spawned a bit later, when
  *66-scandir*(1) is executed).
- It forks a child, also called _stage2_:
	- The child blocks until the catch-all logger runs.
	- The child starts service of tree _TREE_.
	- The child becomes a session leader.

- It also makes the catch-all logger's fifo its stderr.
- It execs into "66-scandir -v VERBOSITY -l LIVE -u" with *LIVE/scandir/0*
  (default */run/66/scandir/0*) as its scandir.

- *66-scandir*(1) pass the relay to *s6-svscan* which spawns the early
  services that are defined in _TREE_ where one of those services is
  _scandir-log_, which is the catch-all logger. When this service is up,
  *66-boot*'s child unblocks.
- The child execs into _rc.init_.

If one of the above processus fail, *66-boot* try to launch a Single-user login
a.k.a. _sulogin_ which let you the ability to fix the error. This case should
not arrive.

# EXIT CODES

*66-boot* never exits. It spawns the _rc.init_ script and execs
*66-scandir*(1), which runs forever until the machine stops or reboots.

# SKELETON FILES

Skeleton files are mandatories and must exist on your system to be able to boot
and shutdown the machine properly. By default those files are installed at
*%%sysconfdir%%*. Use the *--sysconfdir=*_DIR_ option at compile time to change
it.

- *init*++
*66-boot* binary is not meant to be called directly, or be linked to the
  binary directory directly, because it takes command-line options. Thereby
  the _init_ file is used to pass an options to *66-boot*. By default
  *66-boot* is launched with *-m* options. This file *should be copied* by
  the administrator into the binary directory of your system.

- *init.conf*++
this file contains a set of _key_*=*_value_ pairs. *All* keys are mandatory
  and thereby the name of the key *cannot* be changed. This is the file used
  configure the boot process. By default:

	- *VERBOSITY=*_0_++
increase/decrease the verbosity of _stage1_ process.

	- *LIVE=*_/run/66_++
create the scandir at _LIVE_. The value by default will depend of
	  the *--livedir=*_live_ set a compile time. An absolute is expected
	  as value.

	- *PATH=*_/usr/bin:/usr/sbin:/bin:/sbin:/usr/local/bin_++
the initial value for the *PATH* environment variable, that will be
	  transmitted to all the starting process unless it's overridden by a
	  *PATH* declaration via the *-e* option. It is absolutely necessary
	  for *execline*, *s6*, *s6-rc* and all *66 tools* binaries to be
	  accessible via *PATH*, else the machine will not boot.

	- *TREE=*_init_++
name of the _tree_ to start. This _tree_ should contain a set of
	  service to bring up the machine to an operating system. 'classic'
	  services will become earlier service at *66-init* invocation.
	  _stage2_ will start all other kind of service defined into it. It's the
	  responsability of the administrator to correctly set this tree.

	- *RCINIT=*_/etc/66/rc.init_++
This file is launched at the end of the _stage1_ and run as _stage2_.
	  It call *66-init*(1) to iniatiate all service of _TREE_ except
	  'classic' which are already initiated on the _stage1_ than invoke
	  *66-dbctl*(1) to bring up the services. An absolute path is expected as
	  value pointing to the name of the file to run.

	- *RCSHUTDOWN=*/etc/66/rc.shutdown_++
This is launched when a shutdown is requested also called _stage3_. It invoke
	  *66-all*(1) to bring down all services of _TREE_. An absolute path is
	  expected as value pointing to the name of the file to run.

	- *UMASK=*_0022_++
Set the value of the initial file umask for all the starting processes, in
	  octal.

	- *RESCAN=*_0_++
Ask at *s6-svscan* to perform a scan every _RESCAN_ milliseconds. It should
	  be 0 at _stage1_ but its here just in case. It is strongly discouraged to
	  set _RESCAN_ to a positive value under 500.

	- *ISHELL=*_/etc/66/ishell_++
Run _ISHELL_ in case of _stage2_ crash. This file try to run a *sulogin*. An
	  absolute path is expected as value pointing to the name of the file to run.

- *rc.init*++
This file is called by the child of *66-boot* to process the _stage2_. It
  invoke two command:

	- *66-init -v${VERBOSITY} -l ${LIVE} -t ${TREE} database* will initiate
	  *bundle* and *atomic* service of the _TREE_. If the process fail, it try
	  to run _ISHELL_ as command to rescue.

	- *66-dbctl -v${VERBOSITY} -l ${LIVE} -t ${TREE} -u* will bring up all
	  *bundle* and *atomic* service of the _TREE_. If the process fail, it try
	  to run _ISHELL_ as command to rescue.

- *rc.shutdown*++
This file is called at shutdown request when the administrator runs the
  *shutdown*, *halt*, *poweroff* or *reboot* command.++
It invoke one command:

	- *66-all -v${VERBOSITY} -l ${LIVE} -t ${TREE} -f down* will bring down all
	  _services_ for all _tree_ marked enabled.

- *ishell*++
this file is called by *rc.init* in case of crash of the _stage2_ and try to
  run a *sulogin*.

- *halt*++
Safe wrapper around *66-hpr -h* command and accept any command options of it.
  This is the file called to halt the system and *should be copied or
  symlinked* by the administrator into the binary directory of your system.


- *poweroff*++
Safe wrapper around *66-hpr -p* command and accept any command options of it.
  This is the file called to poweroff the system and *should be copied or
  symlinked* by the administrator into the binary directory of your system.

- *reboot*++
Safe wrapper around *66-hpr -r* command and accept any command options of it.
  This is the file called to reboot the system and *should be copied or
  symlinked* by the administrator into the binary directory of your system.

- *shutdown*++
Safe wrapper around *66-shutdown*(1) command and accept any command options of
  it. This is the file called to shutdown the system and *should be copied or
  symlinked* by the administrator into the binary directory of your system.
