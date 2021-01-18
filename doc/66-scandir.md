title: The 66 Suite: 66-scandir
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# 66-scandir

Handles the *scandir* for a given user. Designed to be either root or a branch of the supervision tree.

## Interface

```
    66-scandir [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -b|B ] [ -c ] [ -L log_user ] [ -s skel ] [ -e environment ] [ -o owner ] create|remove
```

This program creates or remove the *scandir* (directory containing a collection of s6‑supervise processes) for the current owner of the proccess depending on the provided options.

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

- **-l** *live* : changes the supervision directory of *service* to *live*. By default this will be `%%livedir%%`. The default can also be changed at compile time by passing the `--livedir=live` option to `./configure`. An existing absolute path is expected and should be within a writable and executable filesystem - likely a RAM filesystem.

- **-b** : create specific files for boot. Only the root user can use this option. It is not meant to be used directly even with root. [66-boot](66‑boot.html) calls it during the boot process.

- **-B** : create specific files for boot inside a container. This option modifies some behaviors:
    * The ultimate output fallback (i.e. the place where error messages go when nothing catches them, e.g. the error messages from the catch-all logger and the s6-supervise process managing the catch-all logger) is not /dev/console, but the descriptor that was *init*'s standard error.
    * Stopping the container with `reboot` will make the container's init program report being killed by a SIGHUP. Stopping it with `poweroff` will make it report being killed by a SIGINT. This is according to the [reboot(2)](http://man7.org/linux/man-pages/man2/reboot.2.html) specification.
    * Stopping the container with `halt`, however, is different. It will make the container's pid 1 read a number in the `%%livedir%%/container/\<owner\>/halt` file which contents the variable `EXITCODE`, and exit with the code it has read. (Default is 0.) This means that in order to run a command in a container managed by [66-boot](66-boot.html) and exit the container when the command dies while reporting the exit code to its parent, [66-boot](66-boot.html) use the `%%skel%%/rc.init.container` file instead of the `%%skel%%/rc.init` file. This file should be modified to launch the command that you want to start inside this container. Then you need to stop that container calling `halt`.
    * All the running services will be killed, all the zombies will be reaped, and the container will exit with the required exit code.

- **-c** : run the system without a `catch-all` logger. On a non-containerized system, that means that all the logs from the s6 supervision tree will go to /dev/console, and that /dev/console will also be the default stdout and stderr for services running under the supervision tree: use of this option is ***discouraged***. On a containerized system (when paired with the **-B** option), it simply means that these outputs go to the default stdout and stderr given to the container's *init* - this should generally not be the default, but might be useful in some cases.

- **-s** *skel* : an absolute path. Directory containing *skeleton* files. This option is not meant to be used directly even with root. [66‑boot](66-boot.html) calls it during the boot process. Default is `%%skel%%`.

- **-L** *log_user* : will run the `catch-all` logger as *log_user*. Default is `%%s6log_user%%`. The default can also be changed at compile-time by passing the `‑‑with‑s6‑log‑user=user` option to `./configure`.

- **-e** *environment* : an absolute path. Merge the current environment variables of *scandir* with variables found in this directory. Any file in environment not beginning with a dot and not containing the `=` character will be read and parsed.

- **-o** *owner* : handles the *scandir* for the given *owner*. Only the root user can use this option. Note that *owner* can be any valid user on the system. However, the given user must have sufficient permissions to create the necessary directories at its location. That is `%%livedir%%` by default or the resulting path provided by the **‑l** option.

## Scandir creation process

When creating the *scandir* various files and directories will be created at the *live* directory.

If created with the user root, you will find the following in `%%livedir%%` (the directory created by default if **‑l** is not passed and 0 being the corresponding UID for the root user):

- *%%livedir%%/scandir/0* : stores all longrun proccesses (commonly known as daemons) started by root.

- *%%livedir%%/tree/0* : stores any [s6‑rc](https://skarnet.org/software/s6-rc) service database started by root.

- *%%livedir%%/log/0* : stores the `catch-all` logger when the *scandir* is created for a boot procedure with the **‑b** option.

- *%%livedir%%/state/0* : stores internal *66* ecosystem files.

If the *scandir* was created with a regular user you will find the following in `%%livedir%%`
(Default directories if **‑l** is not passed and 1000 being the UID for the user):

- *%%livedir%%/scandir/1000*

- *%%livedir%%/tree/1000*

- *%%livedir%%/state/1000*

The **-B** option create an extra directory at `%%livedir%%/scandir/<owner>/container` containing a file named *halt*. See [66-boot](boot.html) for further information.

If a *scandir* already existed at the default location for the given user it will prevent its creation when calling `66‑scandir create`. If you wanted to create a different scandir for the same owner at the same live location you must delete it first with `66-scandir remove`.

## Scandir removal process

The *scandir* **must** first be stopped sending a signal with [66‑scanctl stop](66-scanctl.html) or similar to be able to remove it.

Some directories of the *scandir* may not be removed if another user accesses them. In our previous example where we created a *scandir* for root with UID `0` and a regular user with the UID `1000` this would imply the following:

- *%%livedir%%/scandir/0* # will be deleted

- *%%livedir%%/tree/0*    # will be deleted

- *%%livedir%%/log/0*     # will be deleted

- *%%livedir%%/state/0*   # will be deleted

The *live* directory of the root user (in this example and by default `%%livedir%%`) will not be removed because another user (the one from our example) can still use it. In fact *66‑scandir* will only remove the subdirectories of the corresponding UID of the *owner* while the *live* root directory is not touched. If *live* was created on a RAM filesystem as suggested the deletion happens on the next reboot.

## Boot specification

The **-b**, **-B**, **-c** and **-s** option are called by [66-boot](66-boot.html). **‑b** and **-B** will create .s6‑svscan control files (see [s6‑svscan](https://skarnet.org/software/s6/s6-svscan.html) interface documentation) specifically for stage1 (PID1 process). This special *scandir* is controlled by the safe wrappers `halt`, `poweroff`, `reboot`, `shutdown` provided with *66* tools. The [66-shutdownd](66‑shutdownd.html) daemon which controls the shutdown request will be created automatically at the correct location. Further this specific task needs to read the skeleton file `init.conf` containing the *live* directory location which is the purpose of the **‑s** option.

The *live* directory for the boot process requires writable directories and an executable filesystem. In order to accommodate for read‑only root filesystems there needs to be a tmpfs mounted before [s6‑svscan](https://skarnet.org/software/s6/s6-svscan.html) can be run.

## Environment configuration

You can modify environment variables when starting the *scandir* with the **‑e** option. This option expects to find a valid absolute path to a directory containing one or more files where the format is the classic `key=value` pair. Each file found will be read and parsed.

Any service launched on the *scandir* will inherit the environment variables of the scandir. A specific global `key=value` pair inherited by all service can be set using this option.

### Limits

The mentioned environment directory ***can not*** exceed more than `100` files. Each file can not contain more than `8095` bytes or more than `50` `key=value` pairs.
