title: The 66 Suite: boot
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# boot

Meant to be run as pid 1 as a *stage1* init. Performs the necessary early system preparation and execs into [scandir start](66-scandir.html).

## Interface

```
boot [ -h ] [ -z ] [ -m ] [ -s skel ] [ -l log_user ] [ -e environment ] [ -d dev ] [ -b banner ]
```

This program performs some early preparations, spawns a process that will run the `rc.init` script and then execs into [scandir start](66-scandir.html).

## Exit codes

Command *boot* never exits. It spawns the `rc.init` script and execs into [scandir start](66-scandir.html) which runs forever until the machine stops or reboots.

## Options

- **-h**: prints this help.

- **-m**: umount the basename of the *LIVE* directory set into the *init.conf* skeleton file, if it is already mounted, and mounts a tmpfs on it. By default, the *LIVE* basename is mounted if it is not already a valid mountpoint. Otherwise without the **-m** option, it does nothing.

- **-s** *skel*: an absolute path. Directory that holds skeleton files. By default this will be `%%skel%%`. The default can also be changed at compile time by passing the `--with-skeleton=DIR` option to `./configure`. This directory ***must*** contain the necessary skeleton files to properly boot the machine, without it the system **will not boot**.

- **-l** *log_user*: the `catch-all` logger will run as *log_user*. Default is `%%s6log_user%%`. The default can also be changed at compile-time by passing the `--with-s6-log-user=user` option to `./configure`.

- **-e** *environment*: an absolute path. *stage 1 init* empties its environment before spawning the `rc.init` skeleton file and executing into [scandir start](66-scandir.html) in order to prevent kernel environment variables from leaking into the process tree. The *PATH* variable is the only variable set for the environment. If you want to define additional environment variables then use this option. Behaves the same as [scandir start -e](66-scandir.html).

- **-d** *dev*: mounts a devtmpfs on *dev*. By default, no such mount is performed - it is assumed that a devtmpfs is automounted on `/dev` at boot time by the kernel or an initramfs.

- **-b** *banner*: prints banner to */dev/console* at the start of the stage 1 init process. Defaults to:
`[Starts stage1 process ...]`

## Early preparation

When booting a system, command *boot* performs the following operations:

- It prints a banner to `/dev/console`.

- It imports the environment variables. See [Environment importation](#environment-importation) below.

- It parses the `init.conf` skeleton file.

- It chdirs into `/`.

- It sets the umask to *initial_umask*.

- It becomes a session leader.

- It mounts a devtmpfs on *dev*, if requested.

- It uses `/dev/null` as its stdin (instead of `/dev/console`). Although stdout and stderr still use `/dev/console` for now.

- It checks if the *LIVE* basename is a valid mountpoint, and if so it mounts it. If requested, it unmounts if the *LIVE* basename is a valid mountpoint and performs a mount.

- It creates the *LIVE* directory invocating [66 -v VERBOSITY -l LIVE scandir  -b -c -s skel create](66-scandir.html) plus **-L user_log** if requested.

- It initiates the early services of *TREE* invocating [66 -v VERBOSITY -l LIVE tree init TREE](66-tree.html#init).

- It performs "the fifo trick" where it redirects its stdout to the `catch-all` logger's fifo without blocking before the `catch-all` logger is even up (because it's a service that will be spawned a bit later, when [scandir start](66-scandir.html) is executed).

- It forks a child, also called *stage2*.

    * The child is blocked until the `catch-all` logger runs.

    * The child starts any service of tree *TREE*.

    * The child becomes a session leader.

- It also makes the catch-all logger's fifo its stderr.

- It execs into [66 -v VERBOSITY -l LIVE scandir start](66-scandir.html) with `LIVE/scandir/0` (default `%%livedir%%/scandir/0`) as its scandir.

    * [scandir start](66-scandir.html) transitions into [s6-svscan](https://skarnet.org/software/s6/s6-svscan.html) which spawns the early services that are defined in *TREE* where one of those services is `scandir-log`, which is the `catch-all` logger. Once this service is up `boot's` command child *stage2* unblocks.

    * The child then execs into `rc.init`

In the unusual event that any of the above processes fail, command *boot* will try to launch a single-user login namely *sulogin* to provide the means to repair the system.

## Skeleton files

Skeleton files are mandatory and must exist on your system to be able to boot and shutdown the machine properly. By default those files are installed at `%%skel%%`. Use the `--with-skeleton=DIR` option at compile time to change it.

- `init` : the command *boot* binary is not meant to be called directly or be linked to the binary directory because it takes command line options. Therefore the `init` skeleton file is used to pass any options to command *boot*. By default command *boot* is launched without options. This file is installed at `%%bindir%%/init`.

- `init.conf` : this file contains a set of `key=value` pairs. ***All*** keys are mandatory where the name of the key ***must not*** be changed. This is the file available to a user to configure the boot process. By default:

    * `VERBOSITY=0` : increases/decreases the verbosity of the *stage1* process.

    * `LIVE=%%livedir%%` : an absolute path; creates the scandir at *LIVE*. The value will depend by default on the `--livedir=live` option set at compile time.

    * `PATH=/usr/bin:/usr/sbin:/bin:/sbin:/usr/local/bin` : the initial value for the *PATH* environment variable that will be passed on to all starting processes unless it's overridden by *PATH* declaration with the **-e** option. It is absolutely necessary for [execline](https://skarnet.org/software/execline/),[s6](https://skarnet.org/software/s6/) and all *66 command* binaries to be accessible via *PATH*, else the machine will not boot.

    * `TREE=boot` : name of the *tree* to start. This *tree* should contain a sane set of services to bring up the machine into an operating system. Service marked `earlier` will start early at the invocation of [tree init](66-tree.html#init) command. *stage2* will then start any other service type. It is the responsibility of the system administrator to build this tree without errors.

    * `RCINIT=%%skel%%/rc.init` : an absolute path. This file is launched at the end of *stage1* and run as *stage2*. It calls [tree init](66-tree.html#init) command to initiate any enabled services inside of *TREE* except the earlier ones which were already initiated by *stage1*. After that it invokes [66 start](66-start.html) command to bring up all services.

    * `RCSHUTDOWN=%%skel%%/rc.shutdown` : an absolute path. This is launched when a shutdown is requested also called *stage3*. It invokes [66 tree stop](66-tree.html) command to bring down all services of *TREE*.

    * `RCSHUTDOWNFINAL=%%skel%%/rc.shutdown.final` : an absolute path. This file will be run at the very end of the shutdown procedure, after all processes have been killed and all filesystems have been unmounted, just before the system is rebooted or the power turned off.

    * `UMASK=0022` : sets the value of the initial file umask for all starting processes in octal.

    * `RESCAN=0` : forces [s6-svscan](https://skarnet.org/software/s6/s6-svscan.html) to perform a scan every *RESCAN* milliseconds. This is an overload function mostly for debugging. It should be 0 during *stage1*. It is strongly discouraged to set *RESCAN* to a positive value smaller than 500.

    * `CONTAINER=0` : accepted value are `0` or `1` where `0` ask to boot on a hardware system and `1` ask to boot inside a container. Default `0`. If set to `1`, the `rc.init.container` file is used instead of the `rc.init` file.

    * `RCINIT_CONTAINER=%%skel%%/rc.init.container` : an absolute path. This is launched when a boot inside a container is asked. It as the same behavior of the `rc.init` file but allows to implement a command to run inside this container and retrieves the exit code of that command. The exit code of the command is automatically written at the `%%skel%%/container/<owner>/halt` file modifying the `EXITCODE=` variable.

    * `CATCHLOG=1` : accepted value are `0` or `1` where `0` ask to not redirects its stdout to the `catch-all` logger's fifo and `1` ask to redirects its stdout to the `catch-all` logger's fifo. Default `1`.

- `rc.init` : this file is called by the child of *boot* command to process *stage2*. It invokes the commands:

    * `66 -v${VERBOSITY} -l ${LIVE} tree start ${TREE}` will initiate and bring up all services marked enabled inside of *TREE*

    * If this commands fail a warning message is sent to sdtout.

- `rc.init.container` : this file replace the `rc.init` when a boot inside a container is asked. It has the same behavior than the `rc.init` file. However, this file is especially designed to be used for a boot inside a container. It allow to easily define a command(see comment on that file) to launch inside the container and to retrieve the exit code of that command.

- `rc.shutdown` : this file is called at shudown when the administrator requests the `halt`, `poweroff` or `reboot` command. It invokes a single command:

    * `66 -v${VERBOSITY} -l ${LIVE} tree stop -f ${TREE}` to bring down all *services* for all *trees* marked as enabled.

## Kernel command line

Any valid `key=value` pair set at the `init.conf` skeleton file can be passed on the kernel command line as parameter:

```
BOOT_IMAGE=../vmlinuz-linux root=/dev/sda3 ro vga=895 initrd=../intel-ucode.img,../initramfs-linux.img TREE=boot VERBOSITY=4
```

## Environment importation

The environment variables used to launch all commands during the boot process are determined in the following order of precedence. For any `key=value` pair, the last one encountered takes precedence:

- Variables imported from the init.conf file.
- Variables imported from the %%environment_adm%% directory.
- Variables imported from the directory specified with the -e option, if provided.
- Variables imported from the kernel command line.