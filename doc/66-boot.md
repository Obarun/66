# boot

Meant to be run as pid 1 as a *stage1* init. Performs the necessary early system preparation and execs into [scandir start](66-scandir.html).

## Interface

```
boot [ -h ] [ -c ] [ -m ] [ -s skel ] [ -l log_user ] [ -e environment ] [ -d dev ] [ -b banner ]
```

This program performs some early preparations, forks a *stage2* process that brings up the enabled trees and then execs into [scandir start](66-scandir.html).

## Exit codes

Command *boot* never exits. It forks *stage2* and execs into [scandir start](66-scandir.html) which runs forever until the machine stops or reboots.

## Options

- **-h, --help**: prints this help.

- **-c, --container**: boot inside a container instead of on real hardware. Container mode is selected **only** by this option. The boot then follows the same path as a hardware boot — it brings up the services of the enabled trees — so a container runs as a full supervised system, not a single command. Leaving the container differs from rebooting a machine: pid 1 exits with a code instead of handing the machine over to the kernel. Use [66 halt](66-halt.html) to make pid 1 exit with the code held in the `%%livedir%%/container/<owner>/halt` file (`EXITCODE`, default `0`); [66 poweroff](66-poweroff.html) and [66 reboot](66-reboot.html) make it report a `SIGINT` and a `SIGHUP` respectively. If the boot itself fails, pid 1 exits with `111`. See the container behaviour under [66 scandir -B](66-scandir.html).

- **-m, --mount**: umount the basename of the *LIVE* directory set into the *init.conf* skeleton file, if it is already mounted, and mounts a tmpfs on it. By default, the *LIVE* basename is mounted if it is not already a valid mountpoint. Otherwise without the **-m** option, it does nothing.

- **-s, --skeleton** *skel*: an absolute path. Directory that holds skeleton files. By default this will be `%%skel%%`. The default can also be changed at compile time by passing the `-D skeleton-dir=DIR` option to `meson setup`. This directory ***must*** contain the necessary skeleton files to properly boot the machine, without it the system **will not boot**.

- **-l, --log-user** *log_user*: the `catch-all` logger will run as *log_user*. Default is `%%66log_user%%`. The default can also be changed at compile-time by passing the `-D 66-log-user=user` option to `meson setup`.

- **-e, --environment** *environment*: an absolute path. *stage 1 init* empties its environment except the *PATH* variable before forking *stage2* and executing into [scandir start](66-scandir.html) in order to prevent kernel environment variables from leaking into the process tree. Then, it import environment from files found at the %%environment_adm%% directory (See [Environment importation](#environment-importation)). If you want to define additional environment variables then use this option. Behaves the same as [scandir start -e](66-scandir.html).

- **-d, --dev** *dev*: mounts a devtmpfs on *dev*. By default, no such mount is performed - it is assumed that a devtmpfs is automounted on `/dev` at boot time by the kernel or an initramfs.

- **-b, --banner** *banner*: prints banner to */dev/console* at the start of the stage 1 init process. Defaults to:
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

- It creates the *LIVE* directory invocating [66 -v VERBOSITY -l LIVE scandir -b -c create](66-scandir.html) plus **-L user_log** if requested.

- It initiates the early services of *TREE* invocating [66 -v VERBOSITY -l LIVE tree init TREE](66-tree.html#init).

- It performs "the fifo trick" where it redirects its stdout to the `catch-all` logger's fifo without blocking before the `catch-all` logger is even up (because it's a service that will be spawned a bit later, when [scandir start](66-scandir.html) is executed).

- It forks a child, also called *stage2*.

    * The child is blocked until the `catch-all` logger runs.

    * The child starts any service of tree *TREE*.

    * The child becomes a session leader.

- It also makes the catch-all logger's fifo its stderr.

- It execs into [66 -v VERBOSITY -l LIVE scandir start](66-scandir.html) with `LIVE/scandir/0` (default `%%livedir%%/scandir/0`) as its scandir.

    * [scandir start](66-scandir.html) transitions into [66-scandir](66-scandir.html) which spawns the early services that are defined in *TREE* where one of those services is `scandir-log`, which is the `catch-all` logger. Once this service is up `boot's` command child *stage2* unblocks.

    * The child then brings up the services of every enabled tree.

In the unusual event that any of the above processes fail, command *boot* will try to launch a single-user login namely *sulogin* to provide the means to repair the system.

## Skeleton files

Skeleton files are mandatory and must exist on your system to be able to boot the machine properly. By default those files are installed at `%%skel%%`. Use the `-D skeleton-dir=DIR` option at compile time to change it.

- `init` : the command *boot* binary is not meant to be called directly or be linked to the binary directory because it takes command line options. Therefore the `init` skeleton file is used to pass any options to command *boot*. By default command *boot* is launched without options. This file is installed at `%%bindir%%/init`.

- `init.conf` : this file contains a set of `key=value` pairs. ***All*** keys are mandatory where the name of the key ***must not*** be changed. This is the file available to a user to configure the boot process. By default:

    * `VERBOSITY=1` : increases/decreases the verbosity of the *stage1* process.

    * `LIVE=%%livedir%%` : an absolute path; creates the scandir at *LIVE*. The value will depend by default on the `-D livedir=live` option set at compile time.

    * `PATH=/usr/bin:/usr/sbin:/bin:/sbin:/usr/local/bin` : the initial value for the *PATH* environment variable that will be passed on to all starting processes unless it's overridden by *PATH* declaration with the **-e** option. It is absolutely necessary for [execline](https://skarnet.org/software/execline/) and all *66 command* binaries to be accessible via *PATH*, else the machine will not boot.

    * `TREE=boot` : name of the *tree* to start. This *tree* should contain a sane set of services to bring up the machine into an operating system. Service marked `earlier` will start early at the invocation of [tree init](66-tree.html#init) command. *stage2* will then start any other service type. It is the responsibility of the system administrator to build this tree without errors.

    * `UMASK=0022` : sets the value of the initial file umask for all starting processes in octal.

    * `CATCHLOG=1` : accepted value are `0` or `1` where `0` ask to not redirects its stdout to the `catch-all` logger's fifo and `1` ask to redirects its stdout to the `catch-all` logger's fifo. Default `1`.

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