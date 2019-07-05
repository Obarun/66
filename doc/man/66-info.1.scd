66-info(1)

# NAME

66-info - Display information about trees and services.

# SYNOPSYS

66-info [ *-h* ] [ *-T* _tree_ ] [ *-S* _service_ ]

## TREE

66-info *-T* [ *-h* ] [ *-c* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ _-r_ ] [ *-d* _depth_ ] _tree_

## SERVICE

66-info *-S* [ *-h* ] [ *-c* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ *-p* _n lines_ ] [ *-r* ] [ *-d* _depth_ ] _service_

# DESCRIPTION

*66-info* displays detailed information about a _tree_ or a specific _service_
depending on the options passed.

# OPTIONS

*-h*
	Prints this help.

*-T*
	Asks for _tree_ information.

*-S*
	Asks for _service_ information.

## TREE

*-h*
	Prints this help.

*-c*
	Disable colorization.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	*1* : (Default) Only print error messages.++
	*2* : Also print warning messages.++
	*3* : Also print debugging messages.

*-l* _live_
	Changes the supervision directory of _service_ to _live_.
	By default this will be *%%livedir%%*. The default can also be changed at
	compile time by passing the --livedir=_live_ option to *./configure*. An
	existing absolute path is expected and should be within a writable
	filesystem - likely a RAM filesystem. See *66-scandir*(1).

*-r*
	Shows the dependency graph of _tree_ in reverse mode.

*-d* _depth_
	Limits the depth of the dependency graph visualisation; defaults to 1.
	If _tree_ is not specified *66-info* will display information about all
	available trees for the current owner of the process.

## SERVICE

*-h*
	Prints this help.

*-c*
	Disable colorization.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	*1* : (Default) Only print error messages.++
	*2* : Also print warning messages.++
	*3* : Also print debugging messages.

*-l* _live_
	Changes the supervision directory of _service_ to _live_.
	By default this will be *%%livedir%%*. The default can also be changed at
	compile time by passing the --livedir=_live_ option to *./configure*. An
	existing absolute path is expected and should be within a writable
	filesystem - likely a RAM filesystem. See *66-scandir*(1).

*-p* _n lines_
	Prints the _n last_ lines from the associated log file of the _service_.

*-r*
	Shows the dependency graph of _service_ in reverse mode.

*-d* _depth_
	Limits the depth of the dependency graph visualisation; defaults to 1.

# EXAMPLE

## TREE OUTPUT DISPLAY

The command "66-info -T boot" as root user on the _Obarun_ default system
displays the following where *boot* is the tree used to properly boot the
machine:

```
	               boot           
	Initialized: yes | Current: no
	Contains:       | Enabled: no
	├─(0,Enabled,oneshot) filesystem
	├─(0,Enabled,oneshot) hostname
	├─(0,Enabled,bundle) 00
	├─(0,Enabled,oneshot) rofs-cgroups
	├─(0,Enabled,oneshot) rofs-kernmod
	├─(421,Enabled,longrun) udevd-udev
	├─(0,Enabled,oneshot) udevd-udevadm
	├─(0,Enabled,bundle) rofs-Udevd
	├─(0,Enabled,oneshot) rofs-console
	├─(0,Enabled,oneshot) rofs-hardclock
	├─(0,Enabled,oneshot) checkfs-dmraid
	├─(0,Enabled,oneshot) checkfs-btrfs
	├─(0,Enabled,oneshot) checkfs-lvm
	├─(0,Enabled,oneshot) checkfs-fscheck
	├─(0,Enabled,bundle) rofs-Checkfs
	├─(0,Enabled,oneshot) rofs-modules
	├─(0,Enabled,oneshot) rofs-swap
	├─(0,Enabled,oneshot) rofs-kernruntime
	├─(0,Enabled,bundle) all-Rofs
	├─(0,Enabled,oneshot) rwfs-fsrw
	├─(0,Enabled,oneshot) rwfs-tmpdir
	├─(0,Enabled,oneshot) rwfs-iptables
	├─(0,Enabled,oneshot) rwfs-ip6tables
	├─(0,Enabled,oneshot) rwfs-loopback
	├─(0,Enabled,oneshot) rwfs-nofilesystem
	├─(0,Enabled,oneshot) rwfs-localtime
	├─(0,Enabled,oneshot) rwfs-random
	├─(0,Enabled,oneshot) rwfs-logfiles
	├─(0,Enabled,oneshot) rwfs-end
	├─(0,Enabled,oneshot) rwfs-dmesglog
	├─(0,Enabled,oneshot) rwfs-66local
	├─(0,Enabled,oneshot) rwfs-tmpfiles
	├─(0,Enabled,bundle) all-Rwfs
	├─(0,Enabled,oneshot) boot-extra
	├─(0,Enabled,bundle) All
	└─(254,Enabled,classic) tty12

```

By default the first service displayed is the first service started, the second
one is the second started and so on.

The first line give you the name of the _tree_, *Initialized* tells if the
_tree_ was initialized with *66-init*(1) tool, *Current* tells if the _tree_
is the current one or not (see *66-tree*(1) *-c* option) and *Enabled* reveals
the state of the tree(see *66-tree*(1) *-E* option).
For each service the first number found between *()* bracket is the
corresponding pid of the service, the second one is the state of the service,
the type of the service is shown next to it. Finally the name of the service is
displayed.

By default the dependency graph is rendered in order of execution. In this
example the *classic* tty12</em> is the last finished service and *oneshot*
_filesystem_ is the first one executed. You can reverse the rendered order with
the *-r* option.
	
## SERVICE OUTPUT DISPLAY

The command "sudo 66-info -S -d3 00" displays the following where *00* is the
name of the service:

```
	                   00
	on tree : boot
	status : Enabled, nothing to display
	type : bundle
	description : mount filesystem and set the hostname
	source : /etc/66/service/boot/
	run at : /run/66/tree/0/boot/servicedirs/00
	                contents
	├─(0,Enabled,oneshot) filesystem
	└─(0,Enabled,oneshot) hostname
```

Let's take another example, the command "sudo 66-info -S -p5 ntpd" displays
the following:

```
	                   ntpd
	on tree : root
	status : Disabled, up (pid 786) 2380 seconds
	type : classic
	description : ntpd daemon
	source : /etc/66/service/
	run at : /run/66/scandir/0/ntpd
	               dependencies
	└─(785,Disabled,classic) ntpd-log
	                 scripts
	start script : foreground { mkdir -p  -m 0755 ${RUNDIR} } 
	execl-cmdline -s { ntpd ${CMD_ARGS} } 
	               environment
	!RUNDIR=/run/openntpd
	!CMD_ARGS=-d -s
	                 logger
	logger associated : ntpd-log
	log destination : /var/log/66/ntpd 
	                log file
	2019-03-18 06:50:21.749572500  adjtimex returns frequency of 0.000000ppm
	2019-03-18 06:50:21.764241500  ntp engine ready
	2019-03-18 06:50:21.847440500  adjtimex adjusted frequency by 16.428986ppm
	2019-03-18 06:50:21.847479500  set local clock to Mon Mar 18 06:50:21 +11 2019 (offset 0.000000s)

```

In our case :

*ntpd* : name of the service. Main section where general informations is displayed.++
*on tree* : service enable on tree root.++
*status* : status of the service.++
*type* : type of the service.++
*description* : short description of the service.++
*source* : origin of the frontend service file used to enable the service.++
*run at* : location of the service into the scandir.++
*dependencies* : dependencies informations section. This section will display
	a graphical tree on the service dependencies.
*scripts* : scripts informations section. This will display the command
	executed at start and stop process.
*start script* : command line executed at start process. If a stop section was
	set an additionnal field called stop script will be displayed with the
	corresponding command line executed at stop process.
*environment* : environment informations section. Display the environment
	variables set at *%%service_sysconf%%/<service_name>* or
	*$HOME/%%service_sysconf%%/<service_name>* depending of the owner of
	the process.
*logger* : logger informations section.++
*logger associated* : name of the service logger.++
*log destination* : destination of the logger file.++
	last part display the last 5 line of the logger file.

# NOTES

*66-info* use color to output the informations, the previous examples do not
reflect the veritable output concerning the colors.
