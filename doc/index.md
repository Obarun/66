title: The 66 Suite: index
author: Eric Vidal <eric@obarun.org>

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# What is 66?

Sixty-six is a service manager providing init built around [s6](https://skarnet.org/software/s6) created to make the implementation and manipulation of service files on your machine easier. It is meant to be a toolbox for the declaration, implementation and administration of services to achieve powerful functionality with small amounts of code.

Examples of what can be achieved by assembling different programs provided by `66`:

- Frontend service files declaration.
- Easy creation of a scandir.
- Nested supervision tree.
- Instance service file creation.
- Multiple directories service file declaration(packager,sysadmin,user).
- Easy change of service configuration.
- Automatic logger creation even for oneshot service.
- Service Notification.
- Organizes services as a tree.
- Easy view of service status.
- log file in human readable format.
- User service declaration.
- Automatic dependencies service chain.
- ...

`66` can easily be used within container. With one command, [66 scandir start](66-scandir.html), you are ready to go to supervise and manage any kind of service as root or regular account.

Booting with `66` to be able to supervise and manage services **is not** mandatory. You can use it in parallel with another init or service manager, start a `scandir` directory with the [66 scandir start](66-scandir.html) command, and you are ready to supervise any kind of services.

**Note**: This documentation tries to be complete and self-contained. However, if you have never heard of [s6](https://skarnet.org/software/s6) you might be confused at first. Please refer to the skarnet documentation if in doubt.

## Installation

### Requirements

Please refer to the [INSTALL.md](https://git.obarun.org/Obarun/66/-/blob/master/INSTALL.md) file for details.

### Licensing

`66` is free software. It is available under the [ISC license](http://opensource.org/licenses/ISC).

### Upgrade

See [changes](66-upgrade.html) between version.

**(!)** The significant changes in versions 0.7.0.0 and above render them incompatible with versions prior to 0.7.0.0. You can refer to the [Rosetta Stone](66-rosetta.html) to understand the interface and behavioral differences between versions below 0.7.0.0 and version 0.7.0.0

---

## Commands

- [66](66.html)

### Extra tools

- [66-echo](66-echo.html)
- [66-umountall](66-umountall.html)
- [66-nuke](66-nuke.html)
- [execl-envfile](execl-envfile.html)

### Internal tools

- [66-hpr](66-hpr.html)
- [66-shutdownd](66-shutdownd.html)
- [66-shutdown](66-shutdown.html)

### Others documentation

- [frontend service file](66-frontend.html)
- [instantiated service file](66-instantiated-service.html)
- [module service usage](66-module-usage.html)
- [module service creation](66-module-creation.html)
- [Service configuration file](66-service-configuration-file.html)
- [Deeper understanding](66-deeper.html)
- [Upgrade and Migration process](66-upgrade-process.html)

## Why is 66 necessary?

Implementation and handling of service files based on [s6](https://skarnet.org/software/s6) can be complex and difficult to understand. This led to the creation of the `66` program.

Why the name?

Previously `66` was the result of the combination of the former [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc). With time and code improvement the `s6-rc` program was dropped. `66` is now a fully independent service manager, although the name has been retained.
It is a lot faster and easier to write and remember when writing. Apart from that it is a nice command prefix to have. It identifies the origin of the software and it's short.

Expect more use of the `66-` prefix in future [obarun](https://web.obarun.org) software releases and please avoid using it for your own projects.
