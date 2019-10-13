66-intree(1)

# NAME

66-intree - Display information about trees.

# SYNOPSYS

66-intree [ *-h* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ *-c* ] [ *-o* name,init,enabled,... ] [ *-g* ] [ *-d* _depth_ ] [ *-r* ] _tree_

# DESCRIPTION

*66-intree* displays detailed information about a _tree_.

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	*1* : (Default) Only prints error messages.++
	*2* : Also prints warning messages.++
	*3* : Also prints debugging messages.

*-l* _live_
	Changes the supervision directory of _service_ to _live_.
	By default this will be *%%livedir%%*. The default can also be changed at
	compile time by passing the --livedir=_live_ option to *./configure*. An
	existing absolute path is expected and should be within a writable
	filesystem - likely a RAM filesystem. See *66-scandir*(1).

*-c*
	enable colorization.

*-o*
	comma separated list of field to display. If the option is not past,
	*66-intree* will display all field.

*-g*
	shows the dependency list of _tree_ as graph instead of list.

*-r*
	Shows the dependency list of _tree_ in reverse mode.

*-d* _depth_
	Limits the depth of the dependency list visualisation; defaults to 1.
	This implies the *-g* option.

	If _tree_ is not specified *66-intree* will display information about all
	available trees for the current owner of the process.

## VALID FIELD FOR -g OPTION

*name*
	Displays the name of the _tree_.

*init*
	Displays a boolean value of the initialization state.

*enabled*
	Displays a boolean value of the enable state.

*curren*
	Displays a boolean value of the current state.

*contains*
	Displays the contain of _tree_.

# COMMAND AND OUTPUT EXAMPLE

The command "66-intree boot" as root user on _Obarun_'s default system
displays the following where *boot* is the tree used to properly boot 
the machine:

```
Name        : boot
Initialized : yes
Enabled     : no
Current     : no
Contains    : tty12  system-hostname  mount-run  populate-run  mount-tmp
              populate-tmp  mount-proc  mount-sys  populate-sys  mount-dev
              mount-pts  mount-shm  populate-dev  mount-cgroups  00
              modules-kernel  udevd  udevadm  devices-crypttab
              system-hwclock  system-random  modules-system  system-sysctl
              system-fontnkey  devices-dmraid  devices-btrfs  devices-lvm
              devices-zfs  system-Devices  mount-swap  all-Mount
              system-fsck  mount-fstab  all-System  mount-rw  local-iptables
              local-ip6tables  local-loop  local-sethostname  local-time
              local-authfiles  local-tmpfiles  local-rc  local-dmesg  all-Local
              all-Runtime  All
```

The field *name* gives you the name of the _tree_.

The field *Initialized* tells you if the _tree_ was initialized with "66-init" tool.

The field *Enabled* reveals the state of the tree -- see "66-tree -E".

The field *Current* tells you if the _tree_ is the current one or not -- see
"66-tree -c".

The field *Contains* gives you a list of all services enabled in the _tree_.

You can display the contents list as a graph and only these fields using 
the command "66-intree -o contains -g boot":

```
Contains    : /
              ├─(253,Enabled,classic) tty12
              ├─(0,Enabled,oneshot) system-hostname
              ├─(0,Enabled,oneshot) mount-run
              ├─(0,Enabled,oneshot) populate-run
              ├─(0,Enabled,oneshot) mount-tmp
              ├─(0,Enabled,oneshot) populate-tmp
              ├─(0,Enabled,oneshot) mount-proc
              ├─(0,Enabled,oneshot) mount-sys
              ├─(0,Enabled,oneshot) populate-sys
              ├─(0,Enabled,oneshot) mount-dev
              ├─(0,Enabled,oneshot) mount-pts
              ├─(0,Enabled,oneshot) mount-shm
              ├─(0,Enabled,oneshot) populate-dev
              ├─(0,Enabled,oneshot) mount-cgroups
              ├─(0,Enabled,bundle) 00
              ├─(0,Enabled,oneshot) modules-kernel
              ├─(485,Enabled,longrun) udevd
              ├─(0,Enabled,oneshot) udevadm
              ├─(0,Enabled,oneshot) devices-crypttab
              ├─(0,Enabled,oneshot) system-hwclock
              ├─(0,Enabled,oneshot) system-random
              ├─(0,Enabled,oneshot) modules-system
              ├─(0,Enabled,oneshot) system-sysctl
              ├─(0,Enabled,oneshot) system-fontnkey
              ├─(0,Enabled,oneshot) devices-dmraid
              ├─(0,Enabled,oneshot) devices-btrfs
              ├─(0,Enabled,oneshot) devices-lvm 
              ├─(0,Enabled,oneshot) devices-zfs
              ├─(0,Enabled,bundle) system-Devices
              ├─(0,Enabled,oneshot) mount-swap 
              ├─(0,Enabled,bundle) all-Mount
              ├─(0,Enabled,oneshot) system-fsck
              ├─(0,Enabled,oneshot) mount-fstab
              ├─(0,Enabled,bundle) all-System
              ├─(0,Enabled,oneshot) mount-rw
              ├─(0,Enabled,oneshot) local-iptables
              ├─(0,Enabled,oneshot) local-ip6tables
              ├─(0,Enabled,oneshot) local-loop
              ├─(0,Enabled,oneshot) local-sethostname
              ├─(0,Enabled,oneshot) local-time
              ├─(0,Enabled,oneshot) local-authfiles
              ├─(0,Enabled,oneshot) local-tmpfiles  
              ├─(0,Enabled,oneshot) local-rc
              ├─(0,Enabled,oneshot) local-dmesg
              ├─(0,Enabled,bundle) all-Local
              ├─(0,Enabled,oneshot) all-Runtime
              └─(0,Enabled,bundle) All

```
For each service the first number found between *()* parentheses is the 
corresponding pid of the service, the second one is the state of the 
service, the type of the service is shown next to it, and finally the name 
of the service is displayed after the parenthesis ')'.

By default the dependency graph is rendered in the order of execution. In 
this example the *classic tty12* is the first executed service and 
*bundle All* is the last before it finishes. You can reverse the rendered order with
the *-r* option.

You can display the name and current field and only these fields for each 
tree using the command "66-intree -o name,current":

```
Name        : boot
Current     : no

Name        : docker
Current     : no

Name        : root
Current     : no

Name        : test
Current     : yes

Name        : user
Current     : no

```
