title: The 66 Suite: 66-intree
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# 66-intree

This command displays information about trees.

## Interface

```
    66-intree [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -n ] [ -o name,init,enabled,... ] [ -g ] [ -d depth ] [ -r ] tree
```

## Exit codes

- *0* success
- *100* wrong usage
- *111* system call failed

## Options

- **-h** : prints this help.

- **-z** : use color.

- **-v** *verbosity* : increases/decreases the verbosity of the command.
    * *1* : only print error messages. This is the default.
    * *2* : also print warning messages.
    * *3* : also print tracing messages.
    * *4* : also print debugging messages.

- **-l** *live* : changes the supervision directory of *service* to *live*. By default this will be `%%livedir%%`. The default can also be changed at compile time by passing the `--livedir=live` option to `./configure`. An existing absolute path is expected and should be within a writable and executable filesystem - likely a RAM filesystem—see [66-scandir](66-scandir.html).

- **-n** : do not display the field name(s) specified.

- **-o** : comma separated list of fields to display. If this option is not passed, *66-intree* will display all fields.

- **-g** : shows the dependency list of the *service* as a hierarchical graph instead of a list.

- **-d** *depth* : limits the depth of the dependency list visualisation; default is 1. This implies **-g** option.

- **-r** : shows the dependency list of *tree* in reverse mode.

***(!)*** If tree is not specified *66-intree* will display information about all available trees for the current owner of the process.

## Valid fields for -o options

- *name* : displays the name of the *tree*.
- *init* : displays a boolean value of the initialization state.
- *enabled* : displays a boolean value of the enable state.
- *start* : displays the list of *tree*(s) started before.
- *current* : displays a boolean value of the current state.
- *allowed* : displays the list of allowed user to use the *tree*.
- *symlinks* : displays the target of *tree*'s symlinks.
- *contents* : displays the contents of the *tree*.

## Command and output examples

The command `66-intree boot`, run as root user, on [Obarun](https://web.obarun.org)'s default system displays the following where *boot* is the tree used to properly boot the machine:

```
    Name         : boot
    Initialized  : yes
    Enabled      : no
    Starts after : None
    Current      : no
    Allowed      : root
    Symlinks     : svc->source db->source
    Contents     : tty12  system-hostname  mount-run  populate-run  mount-tmp  populate-tmp  mount-proc  mount-sys
                   populate-sys  mount-dev  mount-pts  mount-shm  populate-dev  mount-cgroups  00  modules-kernel  udevd
                   udevadm  devices-crypttab  system-hwclock  system-random  modules-system  system-sysctl
                   system-fontnkey  devices-dmraid  devices-btrfs  devices-lvm  devices-zfs  system-Devices  mount-swap
                   all-Mount  system-fsck  mount-fstab  all-System  mount-rw  local-iptables  local-ip6tables  local-loop
                   local-sethostname  local-time  local-authfiles  local-tmpfiles  local-rc  local-dmesg  all-Local
                   all-Runtime  All
```

The field *name* gives you the name of the tree.

The field *Initialized* tells you if the tree was initialized with [66-init](66-init.html) tool.

The field *Enabled* reveals the state of the tree—see [66-tree -E](66-tree.html).

The field *Starts after* reveals the start process order if the *tree* is enabled, meaning which *tree* is started before the current one.

The field *Current* tells you if the tree is the current one or not—see [66-tree -c](66-tree.html).

The field *Allowed* gives you a list of user(s) allowed to handle the tree—see [66-tree -a|b](66-tree.html).

The field *Symlinks* tells you if the current live state point to the *source* or the *backup* of the *tree*. Every use of [66-enable](66-enable.html) tool create a automatic backup of the tree for `classic`,`bundle` or `atomic` service(s). A symlink pointing to *backup* mean that you have enabled a service without starting it. Right after a boot, each tree should point to *source*.

The field *Contents* gives you a list of all services enabled in the *tree*.

You can display the contents list as a graph and only these fields using the command `66-intree -o contents -g boot`:

```
    Contents    : /
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

For each service the first field found between `()` parentheses is the corresponding pid of the service, the second one is the state of the service, and next to it is the type of the service, separated by commas, and finally the name of the service is displayed after the parenthesis `)`.

By default the dependency graph is rendered in the order of execution. In this example the `classic` *tty12* is the first executed service and `bundle` *All* is the last before it finishes. You can reverse the rendered order with the **-r** option.

You can display the name and current field and only these fields for each tree using the command `66-intree -o name,current`:

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
