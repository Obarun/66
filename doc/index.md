title: The 66 Suite: index
author: Eric Vidal <eric@obarun.org>

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# What is 66?

Sixty-six is a collection of system tools built around [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc) created to make the implementation and manipulation of service files on your machine easier. It is meant to be a toolbox for the declaration, implementation and administration of services where seperate programs can be joined to achieve powerful functionality with small amounts of code.

Examples of what can be achieved by assembling different programs provided by *66*:

- Frontend service files declaration.

- Easy creation of a scandir.
- Nested supervision tree.
- Instance service file creation.
- Multiple directories service file declaration(packager,sysadmin,user).
- Easy change of service configuration.
- Automatic logger creation.
- Service Notification.
- Organizes services as a tree.
- Easy view of service status.
- User service declaration.
- Automatic dependencies service chain.
- ...

**Note**: This documentation tries to be complete and self-contained. However, if you have never heard of [s6](https://skarnet.org/software/s6) and/or [s6-rc](https://skarnet.org/software/s6-rc) you might be confused at first. Please refer to the skarnet documentation if in doubt.

## Installation

### Requirements

Please refer to the [INSTALL.md](https://framagit.org/Obarun/66) file for details.

### Licensing

*66* is free software. It is available under the [ISC license](http://opensource.org/licenses/ISC).

---

## Commands

### Debug tools

- [66-parser](66-parser.html)
- [66-inresolve](66-inresolve.html)
- [66-state](66-state.html)

### Admin tools

- [66-boot](66-boot.html)
- [66-scandir](66-scandir.html)
- [66-scanctl](66-scanctl.html)
- [66-init](66-init.html)
- [66-svctl](66-svctl.html)
- [66-dbctl](66-dbctl.html)
- [66-hpr](66-hpr.html)
- [66-shutdownd](66-shutdownd.html)
- [66-update](66-update.html)

### User tools

- [frontend service file](frontend.html)
- [instantiated service file](instantiated-service.html)
- [module service creation](module-service.html)
- [66-all](66-all.html)
- [66-tree](66-tree.html)
- [66-enable](66-enable.html)
- [66-disable](66-disable.html)
- [66-start](66-start.html)
- [66-stop](66-stop.html)
- [66-intree](66-intree.html)
- [66-inservice](66-inservice.html)
- [66-env](66-env.html)
- [66-shutdown](66-shutdown.html)

### Extra tools

- [66-echo](66-echo.html)
- [66-umountall](66-umountall.html)
- [execl-envfile](execl-envfile.html)

## Why is 66 necessary?

Implementation and handling of service files based on [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc) can be complex and difficult to understand. This led to the creation of the 66 tools.
Why the name

*66* is the result of the combination of the former [s6](https://skarnet.org/software/s6) and [s6-rc](https://skarnet.org/software/s6-rc).
It is a lot faster and easier to write and remember when writing. Apart from that it is a nice command prefix to have. It identifies the origin of the software and it's short.

Expect more use of the *66-* prefix in future obarun.org software releases and please avoid using it for your own projects. 
