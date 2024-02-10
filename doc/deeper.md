title: The 66 Suite: deeper understanding
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# Deeper understanding

This documentation explains the internal structure of `66` on the system and the roles of the different directories and file components.

**Never manually changes** any directories or files within the `66` ecosystem,as this is the best way to break it.

## %%system_dir%%

This directory is specified at compile time by using the `--with-system-dir=` option to `./configure`.

This directory stores trees and services configuration files. You should see this following structure

```
%%system_dir%%
└── system
    ├── .resolve
    │   ├── service
    │   │    ├── services -> %%system_dir%%/system/service/svc/<service>
    │   │    └── services -> %%system_dir%%/system/service/svc/<service>
    │   ├── Master
    │   ├── <tree> resolve file
    │   └── <tree> resolve file
    └── service
        └── svc
            ├── <service>
            │   ├── .resolve
            │   │   └── <service> resolve file
            │   ├── state
            │   │   └── status file
            │   ├── file
            │   └── file     
            ├── <services>
            │   ├── .resolve
            │   │   └── <service> resolve file
            │   ├── state
            │   │   └── status file
            │   ├── file
            │   └── file    
            └── <service>
                ├── .resolve
                │   └── <service> resolve file
                ├── state
                │   └── status file
                ├── file
                └── file
```

The exact same structure is also available for regular users at the `${HOME}/%%user_dir%%` directory.

The `%%system_dir%%/system/.resolve` directory contains resolve files for trees. Each tree have its own resolve file, while the [Master](#master-resolve-file) resolve file contains information about all trees available on the system.

The `%%system_dir%%/system/.resolve/service` directory consists of symlinks that point to resolve files for each service. These symlinks enable `66` to quickly access the resolve files for individual services, facilitating the construction of the complete graph of services interdependences.

The `%%system_dir%%/system/service/svc` directory includes service directories, each housing the results of the [parse](66-parse.html) process, as well as internal directories and files essential for `66`.

### %%system_dir%%/system/.resolve/<tree>

The content of the resolve file for a tree, such as `%%default_treename%%`, can be viewed using the [66 resolve](66-resolve.html) command in the following manner

```
66 tree resolve global
```

This displays information in a self-explanatory manner

```
name        : global
depends     : None
requiredby  : session
allow       : 0
groups      : admin
contents    : consolekit consolekit-log dbus dbus-log networkmanager networkmanager-log
ndepends    : 0
nrequiredby : 1
nallow      : 1
ngroups     : 1
ncontents   : 6
```

This information is utilized by `66` to fulfill the user-requested commands. It contains precisely what is necessary and nothing more.

For instance, in a command like `66 tree admin -o depends=security global`, the `security` tree name is appended to the `depends` field, incrementing the `ndepends` count by one.

Essentially, trees are just resolve files - keeping it simple, straightforward, and easy.

#### Master resolve file

The `Master` resolve file holds significant importance as it contains information about all trees. You can access its contents using the `66 tree resolve Master` command, which presents information in a self-explanatory format

```
name        : Master
allow       : root
enabled     : global session
current     : global
contents    : global session boot
nallow      : 1
nenabled    : 2
ncontents   : 3
```

The `Master` resolve file isn't accessible for direct user use. Attempting to execute commands like `66 tree enable Master` will result in an error.

This file serves `66` in rapidly accessing an overview of trees on the system. When a user triggers a `66 tree disable session` command, it updates the `enabled` and `nenabled` fields to reflect the current state of the tree.

### %%system_dir%%/system/service/svc/\<service\>

Every service possesses its individual directory. At its core, this directory houses the outcome of the [parse](66-parse.html) process, heavily reliant on the corresponding [frontend](frontend.html) file. Yet, all service directories invariably include the `.resolve` and `state` subdirectories.

#### %%system_dir%%/system/service/svc/\<service\>/.resolve

This directory stores the resolve file for each service, mirroring how `%%system_dir%%/system/.resolve/` houses the resolve file for a tree. Running `66 resolve \<service\>` showcases the content of this file, presenting information similar to the following

```
name             : openntpd
description      : ntpd daemon
version          : 0.2.1
type             : 0
notify           : 0
maxdeath         : 5
earlier          : 0
hiercopy         : None
intree           : None
ownerstr         : 0
owner            : 0
treename         : global
user             : root
inns             : None
enabled          : 1
home             : /var/lib/66/
frontend         : /etc/66/service/openntpd
servicedir       : /var/lib/66/system/service/svc/openntpd
depends          : sshd openntpd-log
requiredby       : None
optsdeps         : None
contents         : None
ndepends         : 2
nrequiredby      : 0
noptsdeps        : 0
ncontents        : 0
run              : #!/usr/bin/execlineb -P
fdmove 1 0
s6-fdholder-retrieve /run/66/scandir/0/fdholder/s "pipe:66-w-openntpd-log"
fdswap 0 1
./run.user

run_user         : #!/usr/bin/execlineb -P
fdmove -c 2 1
execl-envfile -v4 /etc/66/conf/openntpd/version

    execl-toc -d /var/empty/openntpd
    execl-toc -d ${socket_dir}
    execl-cmdline -s { ntpd ${cmd_args} }


run_build        : auto
run_shebang      : None
run_runas        : None
finish           : #!/usr/bin/execlineb -S0
fdmove 1 0
s6-fdholder-retrieve /run/66/scandir/0/fdholder/s "pipe:66-w-openntpd-log"
fdswap 0 1
./finish.user $@

finish_user      : #!/usr/bin/execlineb -P
fdmove -c 2 1
execl-envfile -v4 /etc/66/conf/openntpd/version
 s6-rmrf ${socket_dir}/${socket_name}

finish_build     : auto
finish_shebang   : None
finish_runas     : None
timeoutkill      : 0
timeoutfinish    : 0
timeoutup        : 0
timeoutdown      : 0
down             : 0
downsignal       : 0
livedir          : /run/66/
status           : /var/lib/66/system/.resolve/service/openntpd/state/status
servicedir       : /run/66/state/0/openntpd
scandir          : /run/66/scandir/0/openntpd
statedir         : /run/66/state/0/openntpd/state
eventdir         : /run/66/state/0/openntpd/event
notifdir         : /run/66/state/0/openntpd/notif
supervisedir     : /run/66/state/0/openntpd/supervise
fdholderdir      : /run/66/scandir/0/fdholder
oneshotddir      : /run/66/scandir/0/oneshotd
logname          : openntpd-log
logdestination   : /var/log/66/openntpd
logbackup        : 3
logmaxsize       : 1000000
logtimestamp     : 3
logwant          : 1
logrun           : #!/usr/bin/execlineb -P
s6-fdholder-retrieve /run/66/scandir/0/fdholder/s "pipe:66-r-openntpd-log"
./run.user

logrun_user      : #!/usr/bin/execlineb -P
fdmove -c 2 1
s6-setuidgid s6log
s6-log -d3 n3 s1000000 /var/log/66/openntpd

logrun_build     : None
logrun_shebang   : None
logrun_runas     : s6log
logtimeoutkill   : 0
logtimeoutfinish : 0
env              :
cmd_args=!-d

# conf_file=!/etc/ntpd.conf

socket_dir=!/run/openntpd


socket_name=!openntpd.sock


envdir           : /etc/66/conf/openntpd
env_overwrite    : 0
configure        : None
directories      : None
files            : None
infiles          : None
ndirectories     : 0
nfiles           : 0
ninfiles         : 0
```

The `66 status <service>` command provides a subset of these information. Too much detail might be overwhelming, so it simplifies the output for ease of use.

Some precision is needed here:

- The intree field corresponds to the `@intree` field in the [frontend](frontend.html) file, while `treename` indicates the current associated tree of the service. Changing the tree with `66 -t <tree> enable <service>` modifies `treename`, leaving `intree` unchanged.

- The `inns` field indicates whether the service is a part of a `module`.

- The `run` and `finish` field contains the content of the `%%system_dir%%/system/svc/service/svc/<service>/run` and `%%system_dir%%/system/svc/service/svc/<service>/finish` file respectively.

- Meanwhile, `run_user`, and `finish_user` fields are derived from the [[start]](frontend.html#section-start) and [[stop]](frontend.html#section-stop) sections in the frontend file. Specifically, `run_user` corresponds to `@execute` in the [[start]](frontend.html#section-start) section, and the others function similarly but for the [[stop]](frontend.html#section-stop) section.

- Other fields like `ownerstr`, `home`, `frontend`, `servicedir`, `livedir`, `status`, `servicedir`, `scandir`, `statedir`, `eventdir`, `notifdir`, `supervisedir`, `fdholderdir`, `oneshotddir`, `logname`, `logwant` and `env_overwrite` are used internally for `66`'s operations.

#### %%system_dir%%/system/service/svc/\<service\>/status

This directory houses a *binary* file named `status`, which `66` uses to track the service's current operational status. Running `66 state <service>` displays output similar to the following

```
toinit          : 0
toreload        : 0
torestart       : 0
tounsupervise   : 0
toparse         : 0
isparsed        : 1
issupervised    : 1
isup            : 1
```

For instance, when executing `66 free <service>`, the `tounsupervise` field switches to `1` at the process start and back to `0` at the end. Additionally, `isup` and `issupervised` also become `0`.

## %%skel%%

This directory is specified at compile time by using the `--with-skeleton=` option to `./configure`.

It holds various configuration files utilized by `66`.

User may want to manage some subdirectories with the exception of the `%%service_admconf%%` below.

### %%service_admconf%%

This directory is specified at compile time by using the `--with-sysadmin-service-conf=` option to `./configure`.

This directory stores the configuration file of a service, which is the outcome of the [[environment]](frontend.html#section-environment) section parsed from the [frontend](frontend.html) file.

The `66 configure <service>` command handles this directory. It's advised for users, including system administrators, to avoid direct interaction with these directories and utilize the [configure](66-configure.html) command instead.

Its subdirectories contain versioned files per service and are self-explanatory.

### %%seed_adm%%

This directory is specified at compile time by using the `--with-sysadmin-seed=` option to `./configure`.

This directory is managed by the user and stores [seed](66-tree.html#seed-files) files, used, for example, by the [tree](66-tree.html) command. If a [seed](66-tree.html#seed-files) file exists both in this directory and at `%%service_system%%`, the one in this directory takes precedence.

It's crucial for system administrators to avoid altering this directory or its subdirectories, ensuring users can manage it without losing their changes.

### %%service_adm%%

This directory is specified at compile time by using the `--with-sysadmin-service=` option to `./configure`.

This directory is designated for users to store [frontend](frontend.html) files. If a [frontend](frontend.html) file for a service exists both in this directory and at `%%service_system%%`, the file in this directory takes precedence.

It's crucial for system administrators to avoid altering this directory or its subdirectories, ensuring users can manage it without losing their changes.

## %%service_system%%

This directory is specified at compile time by using the `--with-system-service=` option to `./configure`.

This directory is reserved for system administrators to install [frontend](frontend.html) file, often through package manager. Any modifications made by users may be overwritten during system updates.

## %%script_system%%

This directory is specified at compile time by using the `--with-system-script=` option to `./configure`.

This directory serves as a location for system administrators to install additional scripts required by [frontend](frontend.html) files or those related to the `module` service type. It's a secure area for administrators to manage scripts effectively.

## %%seed_system%%

This directory is specified at compile time by using the `--with-system-seed=` option to `./configure`.

this directory is intended for system administrators to install [seed](66-tree.html#seed-files) files. User modifications in this directory may be lost during system updates.

## %%livedir%%

This directory is specified at compile time by using the `--livedir=` option to `./configure`. It also can be specified on-the-fly with the [66 -l](66.html) option.

This should be within a writable and executable filesystem, likely a RAM filesystem, mounted with `exec` and `rw` mount flag.

This directory and its subdirectories are managed by `66`. Users, including system administrators, should avoid directly interacting with these directories.

It create at [66 scandir create](66-scandir.html#start) invocation if it doesn't yet.

### %%livedir%%/log

This directory houses log files, usually one per service. It's commonly used for services initiated before a rewritable `/var/log` directory becomes available.

### %%livedir%%/log/0

This directory stores logs generated by `scandir-log` service, which is internally created by `66`. During the boot, the [rc.init](66-boot.html#skeleton-files) skeleton file performs actions and captures output using `scandir-log`.  It might also contain `uncaught-log` entries from services lacking their own logger, making it valuable for debugging purposes.

### %%livedir%%/scandir

This directory is managed through the [scandir](66-scandir.html) comand. Users, including system administrators, should avoid directly interacting with these directories.

#### %%livedir%%/scandir/UID

Given `66` can run with root or regular account privileges, this directory contains subdirectories. Each account possesses its own scandir specified by its number. For example, `%%livedir%%/scandir/0` is owned by root, while `%%livedir%%/scandir/1000` typically belongs to the first regular account created on the system.

This directory consists of service symlinks that point to its corresponding `%%livedir%%/state/UID/<service>` directory.

*note*: The `66 scandir create -B` invocation create the directory `%%livedir%%/scandir/UID/container` containing a named file *halt*. See [boot](66-boot.html) for further information.

### %%livedir%%/state

This directory is managed internally by `66` and contains directories and files needed for the run time of services. Users, including system administrators, should avoid directly interacting with these directories.

### %%livedir%%/state/UID

As `66` can be executed with root or regular account privilegies, this directory contains subdirectories. Each account has its own `state` specified by its number. For intance, `%%livedir%%/state/0` is owned by root, whereas `%%livedir%%/state/1000` is typically owned by the first regular account created on the system.

For instance, the `%%livedir%%/state/0/<service>` contains a verbatim copy of the `%%system_dir%%/system/service/svc/<service>` for each root service.

Invocating the `66 free <service>` remove the corresponding `%%livedir%%/state/0/<service>` directory.

At [start](66-start.html) command executed will created the corresponding `%%livedir%%/state/0/<service>` if it doesn't exist yet.

## %%system_log%%

This directory is specified at compile time by using the `--with-system-log=` option to `./configure`.

This directory is automatically managed by `66`. Users, including system administrators, should avoid directly interacting with these directories.

Each service, if a logger is associated, has its own subdirectory likely `%%system_log%%/66/<service>`.

User can control the rotation of the log file with:

- the `@backup` field at the [[logger]](frontend.html#section-logger) section of the [frontend](frontend.html) file.

- invocating `66 reload -P <service>` or `66 signal -aP <service>` command.

## Resolve files

*Resolve* files are essentially [CDB](http://cr.yp.to/cdb.html) databases. They are independent of extra libraries, running daemons, or third-party programs. These files are lightweight and efficient to read and write. However, the downside is the inability to upgrade a field without rewriting the entire database whenever modifications are made to any fields.

The size of the database varies based on its contents and typically ranges between 4-7 kilobytes.
