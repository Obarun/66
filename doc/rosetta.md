title: The 66 Suite: rosetta
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

This documentation explains the differences in options and interface changes between versions `v0.6.1.3` and `0.7.0.0`. Futhers information can be found to the [upgrade](upgrade.html) page concerning behavior changes, frontend `@key` changes and more.

# General Interface changes

Generally, the prefix is removed from commands, for example, `66-enable` becomes `66 enable`, `66-start` becomes `66 start`, and so on except for a few special commands -- see below.

## Debug command

| Old command | New command |
| --- | --- |
| `66-parser` | `66 parse` |
| `66-inresolve` | `66 resolve` |
| `66-instate` | `66 state` |

## Admin command

| Old command | New command |
| --- | --- |
| `66-boot` | `66 boot` |
| `66-scandir` | `66 scandir` |
| `66-scanctl` | incorporate into `66 scandir` |
| `66-init` | `66 init` |
| `66-svctl` | `66 signal` |
| `66-dbctl` | removed |
| `66-hpr` | `66-hpr`. This command should not be used directly. Prefer using `66 poweroff`, `66 reboot`, `66 halt` to power off, reboot and halt the machine respectively |
| `66-shutdownd` | `66-shutdownd` |
| `66-update` | removed |

## User command

| Old command | New command |
| --- | --- |
| `66-all` | `66 tree`|
| `66-tree` | `66 tree`|
| `66-enable` | `66 enable`|
| `66-disable` | `66 disable`|
| `66-start` | `66 start`|
| `66-stop` | `66 stop`|
| `66-intree` | `66 tree status`|
| `66-inservice` | `66 status`|
| `66-env` | `66 configure`|
| `66-shutdown` | `66-shutdown`. This command should not be used directly. Prefer using `66 poweroff`, `66 reboot`, `66 halt` to power off, reboot and halt the machine respectively |

## Extra tools

| Old command | New command |
| --- | --- |
| `66-echo` | `66-echo` |
| `66-nuke` | `66-nuke` |
| `66-umountall` | `66-umountall` |
| `execl-envfile` | `execl-envfile` |

## New command

| Command | Action |
| --- | --- |
| `66 reload` | reload a service |
| `66 restart` | restart a service |
| `66 free` | unsupervise a service |
| `66 reconfigure` | parse again a service |
| `66 remove` | remove a service |
| `66 poweroff` | poweroff the system |
| `66 reboot` | reboot the system |
| `66 halt` | halt the system |
| `66 version` | get the version of 66 |

# General options changes

Previous tools accepted the options `-h`, `-z`, `-l`, `-T` and `-t`. These options are now integrated into the general `66` command. Let's take an examples:

| Old command | New command |
| --- | --- |
| `66-enable` -t root consolekit | `66` -t root `enable` consolekit |
| `66-start` -l /run/66 sshd | `66` -l /run/66 `start` sshd |

# Help accessibility

All commands and subcommands accept the `-h` option to access their respective help.

- `66` -h: get the help of the general command
- `66` tree -h: get the help of the tree subcommand
- `66` tree resolve -h: get the help of `resolve` subcommand of the `tree` subcommand

# General exit codes

All commands and subcommands return:

- 0 success
- 100 wrong usage
- 111 system call failed

# Interface and Options changes by command

If not specified, the interface (except for the name itself -- see [General Interface changes](#General-Interface-changes)) or the options do not change.

## 66-scandir
---

`66-scandir` and `66-scanctl` was combined together into the command `66 scandir`.

| old options | new options |
| --- | --- |
| `66-scanctl` interrupt | `66` scandir stop |
| `66-scanctl` reload | `66` scandir reconfigure |
| `66-scanctl` reboot | removed |
| `66-scanctl` poweroff | removed |

New subcommands was integrated to `66 scandir` to avoid using options from `s6-svscanctl`.

| `66` scandir | `s6-svscanctl` |
| --- | --- |
| `66` scandir rescan | `s6-svscanctl` -a |
| `66` scandir abort | `s6-svscanctl` -b |
| `66` scandir nuke | `s6-svscanctl` -n |
| `66` scandir annihilate | `s6-svscanctl` -N |
| `66` scandir zombies | `s6-svscanctl` -z |

## 66-init
---

As `s6-rc` was dropped, the specification of the type of the service is not needed anymore. This program was integrated to `66 tree` command -- see [66 tree init](tree.html#init) documentation for futhers information.

| old argument | new argument |
| --- | --- |
| `66-init` classic | `66` tree init *treename* |
| `66-init` database | `66` tree init *treename* |
| `66-init` both | `66` tree init *treename* |

## 66-svctl
---

This tool previously deals only with `classic` service whereas `66 signal` deals with all kind of services type.

| old options | new options |
| --- | --- |
| `66-svctl` -n | removed |
| `66-svctl` -X | `66` signal -x -d |
| `66-svctl` -K | `66` signal -k -d |

## 66-all
---

This tool previously handles all services for trees, the new command handles all **enabled** services for trees.

| old argument | new argument |
| --- | --- |
| `66-all` up | `66` tree start |
| `66-all` down | `66` tree stop |
| `66-all` unsupervise | `66` tree free |

## 66-tree
---

| old options | new options |
| --- | --- |
| `66-tree` -n | `66` tree create |
| `66-tree` -a | `66` tree admin -o allow=*user*,*user*,... or at creation with `66` tree create -o allow=*user*,*user*,... |
| `66-tree` -d | `66` tree admin -o deny=*user*,*user*,... or at creation with `66` tree create -o deny=*user*,*user*,... |
| `66-tree` -c | `66` tree current |
| `66-tree` -S | `66` tree admin -o depends=*tree*,*tree*,... or at creation with `66` tree create -o depends=*tree*,*tree*,... |
| `66-tree` -E | `66` tree enable |
| `66-tree` -D | `66` tree disable |
| `66-tree` -R | `66` tree remove |
| `66-tree` -C | removed |

## 66-enable
---

With the previous version, services must be enabled to be able to start the service. It is not mandatory anymore with the new version. A service can be started without enabling it first. As result, the enable process is not responsible of the parse process even if it parse the service if the service was never parsed before.

| old options | new options |
| --- | --- |
| `66-enable` -f | `66` enable |
| `66-enable` -F | `66` reconfigure |
| `66-enable` -I | `66` parse -I |
| `66-enable` -S | `66` enable -S |

## 66-disable
---

| old options | new options |
| --- | --- |
| `66-disable` -S | `66` disable -S |
| `66-disable` -F | removed |
| `66-disable` -R | `66` remove |

## 66-start
---

| old options | new options |
| --- | --- |
| `66-start` -r | `66` reload |
| `66-start` -R | `66` restart |

## 66-stop
---

| old options | new options |
| --- | --- |
| `66-stop` -u | `66` free |
| `66-stop` -X | `66` signal -x |
| `66-stop` -K | `66` signal -k -d |

# Converting service frontend file from 0.6.2.0 version to 0.7.0.0

Manual intervention is required to upgrade the frontend file to version 0.7.0.0, as both the `longrun` and `bundle` types have been eliminated. Additionally, certain fields have been altered, deprecated, or removed.

- The `longrun` type should be converted to the `classic` type. To achieve this, simply replace `@type=longrun` with `@type=classic`. This conversion is performed by 66 during the parse process, but the change is not applied to the frontend file.

- The `@extdepends=` field has been removed and has no effect. To address this, switch the service definition in this field to the `@depends=` field. For example, a service declaring `@depends=(consolekit)` and `@extdepends=(dbus)` should be converted by removing the `@extdepends` field and appending dbus to the `@depends=(dbus consolekit)` field.

- The `@shebang` field is deprecated and will be removed in a future release. To define the shebang for our script, place it at the beginning of the `@execute` field **right after** the opened parentheses, **without** any space, new line, or other characters. For example, for an sh script, the beginning of the @execute field must be

    ```
    @execute = (!#/usr/bin/sh
    echo myscript
    ....
    )
    ```

    The incorret way is

    ```
    @execute = (
    #!/usr/bin/sh
    echo myscript
        ....
    )
    ```

    The following is also incorrect

    ```
    @execute = ( #!/usr/bin/sh
    echo myscript
        ....
    )
    ```

    Failure to adhere to the correct syntax will result in an `Error format` during the execution of the service.


- The `bundle` type no longer exists. To achieve a similar behavior, you can emulate the functionality of a `bundle` by using a `oneshot` type. Define the `@depends=` field with the service name previously defined in your `bundle` type under the `@contents` field. The `@execute` field of the `oneshot` service should simply be defined as `true`. For instance, the following bundle

    ```
    [main]
    @type=bundle
    @version=0.0.1
    @description="launch network"
    @user=(root)
    @contents=(connmand openntpd)
    ```
    will be converted to following `oneshot` service
    ```
    [main]
    @type=oneshot
    @version=0.0.1
    @description="launch network"
    @user=(root)
    @depends=(connmand openntpd)

    [start]
    @execute=(true)
    ```

    Certainly, you can replace the `bundle` type with the `module` type. However, depending on your specific cases, the `module` type might overcomplicate the service itself.

- The `module` type must be redefined from scratch due to significant changes in the directory `module` structure.. Refers to the [module-creation](module-creation.html) documentation for detailed information.

# Cleaning the 66 directories and files

The heart structure of `66` has been reframed, resulting in a simplified directory architecture. Clean unused files and directories from the previous release by following the instructions below. Additionally, refer to the [deeper understanding](deeper.html) documentation about the `66` architecture.

## %%system_dir%%

You can safely remove the following directories and files:

- `%%system_dir%%/current` directory.

- `%%system_dir%%/system/`:

    Any other directories and files ***except*** for the `%%system_dir%%/.resolve` and `%%system_dir%%/service` directories from this directory.

The exact same task applies to the `${HOME}/%%user_dir%%` directory. Also, you can remove the `${HOME}/%%user_dir%%/module` directory.

## %%skel%%

You can safely remove the following `%%skel%%/module` directory.