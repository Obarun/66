![latest release](https://git.obarun.org/Obarun/66/-/badges/release.svg)

# 66 - An Independent Linux Service Manager

Sixty-six (66) is a complete Linux service manager. Services are declared in a readable INI format and compiled ahead of time into a per-service database, so the dependency graph is fixed before anything starts rather than recomputed at every boot. Each service gets its own supervisor process, which means supervision survives a crash of the manager itself. 66 can bring up the whole machine as PID 1, or sit alongside another init, in a container as readily as on hardware.

The supervision model is inherited from s6, which 66 was built on for its first decade. Since 0.9.0.0 the scanner, the supervisor, the logger, the fd holder and the privilege dropper are native programs of the suite, and `oblibs` is the only library 66 links against.

## What a service looks like

A service is one file. Here is a supervised daemon that also restarts itself whenever the DHCP client rewrites `/etc/resolv.conf`:

```ini
[Main]
Type = classic
Description = "dnsmasq daemon"
Depends = ( network )

[Start]
Execute = ( /usr/bin/dnsmasq -k )

[Event]
EventType = inotify
From = ( resolv-watch )
Do = restart
```

Everything about dnsmasq lives in the file called dnsmasq, including how it reacts. There is no companion unit beside it.

The `[Event]` block names a source, and a source is itself a service, of a type that runs no process and only emits:

```ini
[Main]
Type = event
Description = "watch /etc/resolv.conf"
EventType = inotify
Watch = /etc/resolv.conf
On = ( IN_CLOSE_WRITE )
```

Bring it up with `66 start dnsmasq`. A logger is attached by default, so `66 status dnsmasq` reports the pid, the uptime, the tree the service landed in, and the last lines it printed.

`Execute` is an [execline](http://skarnet.org/software/execline) script by default, but a shebang on its first line switches language: the same daemon written in bash or Python is the same file with `Execute = (#!/usr/bin/bash …)`.

## Key Features of 66 (not exhaustive):

- **Frontend Service Files Declaration**: Service files are written in an INI format, making them straightforward to read and edit.
- **Supervision for every user, not just root**: each user gets a supervision tree of their own, set up with a single command. Root's tree and a user's tree are separate things.
- **Users cannot disturb each other**: a user's services run and are supervised under that user's own account, with no privileges over root's services or over another user's.
- **Instance Service File Creation**: Supports instantiated service.
- **Identifier Interpretation**: One generic frontend file adapts to whoever owns it. Identifiers such as `@U`, `@H` or `@R` stand for the user's name, home or runtime directory, and are resolved both when the file is parsed and later at runtime, when an environment is loaded, so owner-specific values are never baked in where they should not be.
- **Service Configuration Changes**: Includes built-in versioning for configuration files, including environment variables, to streamline service updates and changes.
- **Automatic Logger Creation (not mandatory)**: Automatically creates dedicated loggers for each service, covering all service types.
- **Help on I/O Redirection**: Provides keywords in frontend files for easy control over standard input, output, and error redirection.
- **Service Notification**: Ensures services are fully ready before managing their dependency chains, using a readiness notification mechanism.
- **Service Organization as a Tree**: Allows quick management and visualization of service groups within a tree structure.
- **Service Status Overview**: Offers a comprehensive set of tools to monitor the state of services and access detailed information easily.
- **User Service Declaration**: Users can declare and manage their own services, facilitating personalized service management.
- **Automatic Dependency Chains**: Automatically handles and maintains service dependencies, ensuring smooth and reliable service operations.
- **Service Order Dependencies**: Guarantees reliable, stable, and reproducible service order dependencies to maintain consistent service behavior.
- **Snapshot Management**: Allows the creation and management of snapshots of your service system, enabling easy backup, recovery, and sharing of service states across multiple hosts.
- **Event-Driven Reactions**: Services can react to what happens elsewhere on the system, another service going up, down or crashing, a routed signal, a filesystem change, a timer, a cron schedule, or an event raised by hand or by another service, and respond by running a `66` command on themselves or by emitting further events. A dedicated `66-eventd` daemon evaluates these rules, so services start, stop or reconfigure dynamically instead of every condition being hard-coded into the service manager.

## Behavior Benefits:

- **No Reboot Required During Upgrades**: Service updates do not require system reboots, ensuring continuous operation.
- **Independent of Boot Management**: 66 can supervise services independently of the boot process, making it optional to use 66 from startup. It is also fully compatible with virtualization platforms like containerd and Docker, allowing for easy monitoring of services within containers.
- **Readable Logs**: Logs are stored in a human-readable format for easier analysis and debugging.
- **File Descriptor Holding for Log Pipes**: Utilizes file descriptor holding for efficient log piping, enhancing reliability and performance.

66 focuses on mechanisms, not policies, and can be compiled with either `glibc` or `musl` for flexibility across different systems.

## Installation

See the INSTALL.md file.

## Documentation

Online [documentation](https://docs.obarun.org/66/latest)

## Contact information

* Email:
  Eric Vidal `<eric@obarun.org>`

* Web site:
  https://web.obarun.org/

* XMPP Channel:
  obarun@conference.xmpp.obarun.org


## Supports the project

Please consider to make [donation](https://web.obarun.org/donate/)

## Frontend service file

66 does not provide any frontend service files by default.

### Boot service file

The boot sequence can be a tedious task to accomplish. A **portable** and **complete** set of services can be found [here](https://git.obarun.org/66-service/arch/boot).
This set of services works out of the box and is highly configurable to suit the needs of the distributions.
POC was made on `Gentoo`, `Funtoo`, `Devuan`, `Void`, `Adelie`, `Antix`, `Arch` and `Obarun`.

### Runtime service file

You can find several examples for common daemons [here](https://git.obarun.org/66-service) for several distributions (Thanks to all contributors).

### Frontend service file scripting

By default, 66 use [execline](http://skarnet.org/software/execline) as scripting language. However, you can specify the scripting language to use.
[66-tools](https://git.obarun.org/obarun/66-tools) provides some additional tools to help you on this task.
Some are specific to `execline` where other can be used on classic shell.

## Roadmap

This Roadmap for the next releases is not written in stone. Feel free to make a merge request to this roadmap.

* [x] Replacement of s6:

  Done in 0.9.0.0. `66-scandir`, `66-supervise`, `66-log` and `66-svctl` replace `s6-svscan`, `s6-supervise`, `s6-log` and `s6-svc`.

* [x] Revise the frontend file's keyword field by excluding the `@` symbol:

  For instance, `@depends` will be `Depends`. That will allow for a file that's closer to the original INI format and less confusing for users.

* [ ] Provide a `[Documentation]` section:

  Enable the provision of documentation for each service using a [Documentation] section. This documentation will be easily accessible by invoking the 66 doc command.

* [x] Provide a `Conflict` keyword at frontend file:

  Allow to declare a conflicting service through the `Conflict` field, e.g. `connmand` service will declare `Conflict = ( networkmanager )`.

* [x] Provide a `Provide` keyword at frontend file:

  Allow to declare an alias service through the `Provide` field, e.g. `connmand` service will declare `Provide = ( Network )`.

* [x] Provide keyword for basic operations:

  Certain repetitive tasks can be more efficiently managed directly by `66` in C rather than scripting them in the `Execute` field. For example, utilizing a `ChangeDirectory` keyword can facilitate moving to the declared WorkDir value before executing the script.

* [x] Reacts on event:

  A service carries its own reaction, in an `[Event]` section of its frontend: what it listens to, and what it does about it. Sources are another service's state, a signal, a filesystem change, a cron expression, a timer, or an event raised by hand. The `66-eventd` daemon evaluates the rules and runs the action on the service that declared it. No second unit beside the service, and no condition hard-coded into the service manager.

* [x] Ability to redirect stdin, stdout and stderr

  Allow to specify to make redirection of standard output

* [x] Ability to Handle a general environment structure

  Every scandir will start with environment variable define by user through configuration file at specific directory, for instance `/etc/66/environment`.

* [x] Ability through a new command to update the general environment:

  A supervision tree is started long before a session exists, and its environment is frozen at that point. `66 env` publishes variables that only a session knows, such as `DISPLAY`, `WAYLAND_DISPLAY` or `XAUTHORITY`, into the runtime environment directory of the tree's owner. Every service picks them up the next time it starts, without declaring anything in its frontend.

* [ ] Provide Hook for boot process

  Allow at specific point of the boot process to execute specific tasks given by user.

* [ ] Extend 66-supervise

  Extend the native `66-supervise` program to include support for a `[Reload]` section, handling tasks before executing the service and managing job events.

* [x] Provide capabilities management

  Implementation of `CapsBound` and `CapsAmbient` to manage bounding set and Ambient capabilities respectively.

* [ ] Provide namespace management

  Implementation of a new `[Namespace]` section to control build and control sandboxing.

* [ ] Provide cgroups management

  Ability to configure and control cgroups of the process and sub-process — per-service resource ceilings (`MemoryMax`, `CPUWeight`, `TasksMax`) and the counters that go with them. Tracking and teardown are already answered by the PID namespace, which is what makes this cheap; the work is priced out in [What cgroups and seccomp would cost 66](what-cgroups-and-seccomp-would-cost.md), together with seccomp.