title: The 66 Suite: 66-scandir
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-scandir

Handles the *scandir* for a given user. Designed to be either root or a branch of the supervision tree.

## Interface

```
    66-scandir [ -h ] [ -z ] [ -v verbosity ] [ -b ] [ -l live ] [ -d notif ][ -t rescan ] [ -L log_user ] [ -s skel ] [ -e environment ] [ -c | r | u ] owner
```

This program creates or starts the *scandir* (directory containing a collection of s6‑supervise processes) for the given *owner* depending on the provided options. Note that *owner* can be any valid user on the system. However, the given user must have sufficient permissions to create the necessary directories at its location. That is `%%livedir%%` by default or the resulting path provided by the **‑l** option. If *owner* is not explicitely set then the user of the current process will be used instead. 

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

- **-b** : create specific files for boot. Only the root user can use this option. It is not meant to be used directly even with root. [66-boot](66‑boot.html) calls it during the boot process.

- **-l** *live* : changes the supervision directory of *service* to *live*. By default this will be `%%livedir%%`. The default can also be changed at compile time by passing the `--livedir=live` option to `./configure`. An existing absolute path is expected and should be within a writable filesystem - likely a RAM filesystem—see [66-scandir](66-scandir.html).

- **-d** *notif* : notify readiness on file descriptor notif. When *scandir* is ready to accept commands from [66‑scanctl](66-scanctl.html), it will write a newline to *notif*. *notif* **cannot be** lesser than `3`. By default, no notification is sent. If **-b** is set, this option have no effects.

- **-t** *rescan* : perform a scan every *rescan* milliseconds. If *rescan* is set to 0 (the default), automatic scans are never performed after the first one and [s6‑svscan](https://skarnet.org/software/s6-svscan) will only detect new services by issuing either [66‑scanctl](66-scanctl.html) reload or [s6‑svscanctl -a](https://skarnet.org/software/s6-svscanctl). It is **strongly** discouraged to set rescan to a positive value under `500`.

- **-s** *skel* : an absolute path. Directory containing *skeleton* files. This option is not meant to be used directly even with root. [66‑boot](66-boot.html) calls it during the boot process. Default is `%%skel%%`.

- **-L** *log_user* : will run the `catch-all` logger as *log_user*. Default is `%%s6log_user%%`. The default can also be changed at compile-time by passing the `‑‑with‑s6‑log‑user=user` option to `./configure`.

- **-e** *environment* : an absolute path. Merge the current environment variables of *scandir* with variables found in this directory. Any file in environment not beginning with a dot and not containing the `=` character will be read and parsed.

- **-c** : create the neccessary directories at *live*. ***Must be*** executed once with this option before being able to use **‑u**.

- **-r** : remove the *scandir* directory at *live*. If it was already started with **‑u** it ***must*** first be stopped sending a signal with [66‑scanctl quit](66-scanctl.html) or similar.

- **-u** : start the *scandir* directory at *live* calling [s6‑svscan](https://skarnet.org/software/s6-svscan).

## Scandir creation process

When creating the *scandir* with the **‑c** option various files and directories will be created at the *live* directory for the given *owner*.

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

If a *scandir* already existed at the default location for the given user it will prevent its creation when calling `66‑scandir ‑c`. If you wanted to create a different scandir for the same owner at the same live location you must delete it first with **‑r**.

## Environment configuration

You can modify environment variables when starting the *scandir* with the **‑e** option. This option expects to find a valid absolute path to a directory containing one or more files where the format is the classic `key=value` pair. Each file found will be read and parsed.

Any service launched on the *scandir* will inherit the environment variables of the scandir. A specific global `key=value` pair inherited by all service can be set using this option.

### Limits

The mentioned environment directory ***can not*** exceed more than `100` files. Each file can not contain more than `4095` bytes or more than `50` `key=value` pairs.

## Scandir removal process

When using the **‑r** option to delete the *scandir* of a given user some directories of the *scandir* may not be removed if another user accesses them. In our previous example where we created a *scandir* for root with UID `0` and a regular user with the UID `1000` this would imply the following:

- *%%livedir%%/scandir/0* # will be deleted

- *%%livedir%%/tree/0*    # will be deleted

- *%%livedir%%/log/0*     # will be deleted

- *%%livedir%%/state/0*   # will be deleted

The *live* directory of the root user (in this example and by default `%%livedir%%`) will not be removed because another user (the one from our example) can still use it. In fact *66‑scandir* will only remove the subdirectories of the corresponding UID of the *owner* while the *live* root directory is not touched. If *live* was created on a RAM filesystem as suggested the deletion happens on the next reboot.

**Note**: A running *scandir* can not be removed. It needs to be stopped first with [66-scanctl](66‑scanctl.html) quit or similar to be able to remove it. 

## Boot specification

The **-b** and **-s** option are called by [66-boot](66-boot.html). **‑b** will create .s6‑svscan control files (see [s6‑svscan](https://skarnet.org/software/s6-svscan.html) interface documentation) specifically for stage1 (PID1 process). This special *scandir* is controlled by the safe wrappers `halt`, `poweroff`, `reboot`, `shutdown` provided with *66* tools. The [66-shutdownd](66‑shutdownd.html) daemon which controls the shutdown request will be created automatically at the correct location. Further this specific task needs to read the skeleton file `init.conf` containing the *live* directory location which is the purpose of the **‑s** option.

The *live* directory for the boot process requires writable directories. In order to accommodate for read‑only root filesystems there needs to be a tmpfs mounted before [s6‑svscan](https://skarnet.org/software/s6-svscan) can be run. 
