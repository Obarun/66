# What is 66?

Sixty-six (66) is a complete Linux service manager: it declares services in a readable INI
format, compiles them ahead of time, and supervises each one with a dedicated supervisor
process. It can bring up a whole machine as PID 1 or run alongside another init, on
hardware or in a container.

This documentation tries to be complete and self-contained. If you are new to process
supervision, some concepts may be unfamiliar at first; the pages below introduce them as
needed.

If you are still deciding whether 66 fits your needs, start with
[where 66 stands among init systems](66-vs-other-init-systems.html): what it looks like in
practice, who it suits, and how it compares with runit, the s6 stack, OpenRC, finit, dinit
and systemd.

## Where to begin

New to 66? Follow these two, in order:

1. **[Getting started](66-getting-started.html)** — write, run and supervise your first service, step by step.
2. **[Cheatsheet](66-cheatsheet.html)** — every day-to-day command on a single page.

Then dig in as your needs grow:

- **Writing service files** — [frontend service file](66-frontend.html), [dependencies and ordering](66-dependencies.html), [logging](66-logging.html), [identifier interpretation](66-identifier.html), [instantiated service](66-instantiated-service.html)
- **Organising services** — [tree](66-tree.html), [module services](66-module.html), [module service usage](66-module-usage.html)
- **Reacting to events** — [the event system](66-event.html), [66-eventd](66-eventd.html)
- **Handing a session's environment over** — [66 env](66-env.html)
- **Coming from another init** — [systemd, OpenRC or runit](66-migration.html), [where 66 stands among them](66-vs-other-init-systems.html)
- **When something breaks** — [troubleshooting & FAQ](66-troubleshooting.html)
- **Administration & boot** — [boot](66-boot.html), [scandir](66-scandir.html), [running in a container](66-container.html), [upgrade and migration](66-upgrade-process.html)
- **Going deeper** — [deeper understanding](66-deeper.html), [standard I/O redirection](66-standard-io-redirection.html), [service configuration file](66-service-configuration-file.html)

## Installation

### Requirements

Please refer to the [INSTALL.md](https://git.obarun.org/Obarun/66/-/blob/master/INSTALL.md) file for details.

### Licensing

`66` is free software. It is available under the [ISC license](http://opensource.org/licenses/ISC).

### Upgrade

See [changes](66-upgrade.html) between versions.

**(!)** The significant changes in versions `0.7.0.0` and above render them incompatible with versions prior to `0.7.0.0`. You can refer to the [Rosetta Stone](66-rosetta.html#changes-between-v0613-and-0700) to understand the interface and behavioral differences between versions below `0.7.0.0` and version `0.7.0.0`

---

## Commands

### Guides

- [Getting started](66-getting-started.html)
- [Cheatsheet](66-cheatsheet.html)
- [Dependencies and ordering](66-dependencies.html)
- [Logging](66-logging.html)
- [The event system](66-event.html)
- [Troubleshooting & FAQ](66-troubleshooting.html)
- [Coming from systemd, OpenRC or runit](66-migration.html)
- [Running 66 inside a container](66-container.html)

### Main command

- [66](66.html)

### Extra tools

- [66-echo](66-echo.html)
- [66-umountall](66-umountall.html)
- [66-nuke](66-nuke.html)
- [execl-envfile](execl-envfile.html)
- [execl-runas](execl-runas.html)

### Internal tools

- [66-supervise](66-supervise.html)
- [66-eventd](66-eventd.html)
- [66-svctl](66-svctl.html)
- [66-hpr](66-hpr.html)
- [66-shutdownd](66-shutdownd.html)

### Others documentation

- [frontend service file](66-frontend.html)
- [instantiated service file](66-instantiated-service.html)
- [module services](66-module.html)
- [module service usage](66-module-usage.html)
- [module service creation](66-module-creation.html)
- [Service configuration file](66-service-configuration-file.html)
- [Deeper understanding](66-deeper.html)
- [Upgrade and Migration process](66-upgrade-process.html)
- [Standard I/O redirection](66-standard-io-redirection.html)
- [Identifier interpretation](66-identifier.html)

## Why is 66 necessary?

Implementing and handling service supervision can be complex and difficult to understand. This led to the creation of the `66` program.

Why the name?

Historically, `66` was built on top of the former `s6` and `s6-rc` programs. With time and code improvement those external dependencies were dropped: `66` is now a fully independent service manager with its own native supervision, although the name has been retained.
It is a lot faster and easier to write and remember when writing. Apart from that it is a nice command prefix to have. It identifies the origin of the software and it's short.

Expect more use of the `66-` prefix in future [obarun](https://web.obarun.org) software releases and please avoid using it for your own projects.
