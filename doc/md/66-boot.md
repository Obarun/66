title: The 66 Suite: 66-boot
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-boot

Meant to be run as pid 1 as a *stage1* init. Performs the necessary early system preparation and execs into [66‑scandir](66-scandir.html).

## Interface


```
    66-boot [ -h ] [ -s skel ] [ -m ] [ -l log_user ] [ -e environment ] [ -d dev ] [ -b banner ]
```


This program performs some early preparations, spawns a process that will run the `rc.init` script and then execs into [66‑scandir](66-scandir.html).

## Exit codes

*66-boot* never exits. It spawns the *rc.init* script and execs into [66‑scandir](66-scandir.html) which runs forever until the machine stops or reboots.

## Options

- **-h** : prints this help. 

- **-s** *skel* : an absolute path. Directory that holds skeleton files. By default this will be `%%skel%%`. The default can also be changed at compile time by passing the `--with-skeleton=DIR` option to `./configure`. This directory ***must*** contain the necessary skeleton files to properly boot the machine, without it the system **will not boot**.

- **-m** : umount the basename of the *LIVE* directory set into the *init.conf* skeleton file if it already mounted and mount a tmpfs on it. By default, the *LIVE* basename is mounted if it not already a valid mountpoint. Otherwise without the **-m** option, it does nothing.

- **-l** *log_user* : the `catch-all` logger will run as *log_user*. Default is `%%s6log_user%%`. The default can also be changed at compile-time by passing the `--with-s6-log-user=user` option to `./configure`.

- **-e** *environment* : an absolute path. *stage 1 init* empties its environment before spawning the `rc.init` skeleton file and executing into [66-scandir](66-scandir.html) in order to prevent kernel environment variables from leaking into the process tree. The *PATH* variable is the only variable set for the environment. If you want to define additional environment variables then use this option. Behaves the same as [66-scandir -e](66-scandir.html).

- **-d** *dev* : mount a devtmpfs on *dev*. By default, no such mount is performed - it is assumed that a devtmpfs is automounted on `/dev` at boot time by the kernel or an initramfs.

- **-b** *banner* : prints banner to */dev/console* at the start of the stage 1 init process. Defaults to:
`[Starts stage1 process ...]`

## Early preparation

When booting a system, *66-boot* performs the following operations:

- It prints a banner to `/dev/console`.

- It parses the `init.conf` skeleton file.

- It chdirs into `/`.

- It sets the umask to *initial_umask*.

- It becomes a session leader.

- It mounts a devtmpfs on *dev*, if requested.

- It uses `/dev/null` as its stdin (instead of `/dev/console`). Although stdout and stderr still use `/dev/console` for now.

- It checks if the *LIVE* basename is a valid mountpoint and if so mounts it. If requested, it unmounts if the *LIVE* basename is a valid mountpoint and performs a mount.

- It creates the *LIVE* directory invocating [66-scandir -v VERBOSITY -l LIVE -b -c -s skel](66-scandir.html) plus **-L user_log** if requested.

- It initiates the early services of *TREE* invocating [66-init -v VERBOSITY -l LIVE -t TREE classic](66-init.html).

- It reads the initial environment from *environment* if requested.

- It performs "the fifo trick" where it redirects its stdout to the `catch-all` logger's fifo without blocking before the `catch-all` logger is even up (because it's a service that will be spawned a bit later, when [66-scandir](66-scandir.html) is executed).

- It forks a child, also called *stage2*.
    
    * The child blocks until the `catch-all` logger runs.
    
    * The child starts any service of tree *TREE*.
    
    * The child becomes a session leader.

- It also makes the catch-all logger's fifo its stderr.

- It execs into [66-scandir -v VERBOSITY -l LIVE -u](66-scandir.html) with `LIVE/scandir/0` (default `%%livedir%%/scandir/0`) as its scandir.
    
    * [66-scandir](66-scandir.html) transitions into [s6-svscan](https://skarnet.org/software/s6/s6-svscan.html) which spawns the early services that are defined in *TREE* where one of those services is `scandir-log`, which is the `catch-all` logger. Once this service is up *66-boot*'s child *stage2* unblocks.
    
    * The child then execs into `rc.init`

In the unusual event that any of the above processes fails *66-boot* will try to launch a single-user login namely *sulogin* to provide a means to repair the system.

## Skeleton files

Skeleton files are mandatory and must exist on your system to be able to boot and shutdown the machine properly. By default those files are installed at `%%skel%%`. Use the `--with-skeleton=DIR` option at compile time to change it.

- `init` : the *66-boot* binary is not meant to be called directly or be linked to the binary directory because it takes command line options. Therefore the `init` skeleton file is used to pass any options to *66-boot. By default *66-boot* is launched with **-m** options. This file ***should be copied*** by the administrator into the binary directory of your system.

- `init.conf` : this file contains a set of `key=value` pairs. ***All*** keys are mandatory where the name of the key ***must not*** be changed. This is the file available to a user to configure the boot process. By default:
    
    * `VERBOSITY=0` : increases/decreases the verbosity of the *stage1* process.
    
    * `LIVE=%%livedir%%` : an absolute path; creates the scandir at *LIVE*. The value will depend by default on the `--livedir=live` option set at compile time.
    
    * `PATH=/usr/bin:/usr/sbin:/bin:/sbin:/usr/local/bin` : the initial value for the *PATH* environment variable that will be passed on to all starting processes unless it's overridden by *PATH* declaration with the **-e** option. It is absolutely necessary for [execline](https://skarnet.org/software/execline/),[s6](https://skarnet.org/software/s6/),[s6-rc](https://skarnet.org/software/s6-rc/) and all *66 tools* binaries to be accessible via *PATH*, else the machine will not boot.
    
    * `TREE=boot` : name of the *tree* to start. This *tree* should contain a sane set of services to bring up the machine into an operating system. 'classic' services will start early at the invocation of [66-init](66-init.html). *stage2* will then start any other service type. It is the responsibility of the system administrator to build this tree without errors.
    
    * `RCINIT=%%skel%%/rc.init` : an absolute path. This file is launched at the end of *stage1* and run as *stage2*. It calls [66-init](66-init.html) to initiate any services inside of *TREE* except 'classic' ones which were already initiated by *stage1*. After that it invokes [66-dbctl](66-dbctl.html) to bring up all services.

    * `RCSHUTDOWN=%%skel%%/rc.shutdown` : an absolute path. This is launched when a shutdown is requested also called *stage3*. It invokes [66-all](66-all.html) to bring down all services of *TREE*.

    * `RCSHUTDOWNFINAL=%%skel%%/rc.shutdown.final` : an absolute path. This file will be run at the very end of the shutdown procedure, after all processes have been killed and all filesystems have been unmounted, just before the system is rebooted or the power turned off.

    * `UMASK=0022` : sets the value of the initial file umask for all starting processes in octal.

    * `RESCAN=0` : forces [s6-svscan]()https://skarnet.org/software/s6/s6-svscan.html) to perform a scan every *RESCAN* milliseconds. This is an overload function mostly for debugging. It should be 0 during *stage1*. It is strongly discouraged to set *RESCAN* to a positive value smaller than 500.
    
    * `ISHELL=%%skel%%/ishell` : an absolute path. Gets called in case *stage2* crashes. This file will try to run a *sulogin*.

- `rc.init` : this file is called by the child of *66-boot* to process *stage2*. It invokes two commands:
    
    * `66-init -v${VERBOSITY} -l ${LIVE} -t ${TREE} database` will initiate `bundle` and `atomic` services inside of *TREE*.
    
    * `66-dbctl -v${VERBOSITY} -l ${LIVE} -t ${TREE} -u` will bring up all `bundle` and `atomic` services inside of *TREE*.
    
    * If any of these two commands fails the *ISHELL* file is called to provide a means of repair.

- `rc.shutdown` : this file is called at shudown when the administrator requests the `shutdown`, `halt`, `poweroff` or `reboot` command. It invokes a single command:
    
    * `66-all -v${VERBOSITY} -l ${LIVE} -t ${TREE} -f down` to bring down all *services* for all *trees* marked as enabled.
    
    * `ishell` : this file is called by `rc.init` in case *stage2* crashes. It will try to run *sulogin* to provide a means of repair.
    
The following skeleton files are called to execute their corresponding power related functions and are safe wrappers that accept their corresponding command options. They **should be copied or symlinked** to the binary directory of the system.

- `halt` : wraps [66-hpr -h](66-hpr.html). Terminates all processes and shuts down the CPU.

- `poweroff` : wraps [66-hpr -p](66-hpr.html). Like `halt` but also turns off the machine itself. Sends an ACPI command to the motherboard then to the PSU to cut off power.

- `reboot` : wraps [66-hpr -r](66-hpr.html). Terminates all processes and instructs a warm boot.

- `shutdown` : wraps [66-shutdown](66-shutdown.html). Like `poweroff` but also runs scripts to bring the system down in a sane way including user notification.
