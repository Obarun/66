title: The 66 Suite: index
author: Eric Vidal <eric@obarun.org>

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# What is 66?

Sixty-six (66) is a service manager designed around the [s6 supervision suite](http://skarnet.org/software/s6) to simplify the implementation and management of service files on your machine. It provides a comprehensive toolbox for declaring, implementing, and administering services with minimal code while delivering powerful functionality.

## Key Features of 66 (not exhaustive):

- **Frontend Service Files Declaration**: Service files are written in an INI format, making them straightforward to read and edit.
- **Simple Scandir Creation**: Easily create [scandir](https://skarnet.org/software/s6/scandir.html) directories for both the root and regular users, allowing for efficient service management across different user levels.
- **Nested Scandir Supervision**: Regular users have their own independent [scandir](https://skarnet.org/software/s6/scandir.html) directories, separate from the root, ensuring user-specific supervision without interference.
- **Instance Service File Creation**: Supports instantiated service.
- **Identifier Interpretation**: Supports specific identifiers that are replaced at parse time to simplify service file creation.
- **Service Configuration Changes**: Includes built-in versioning for configuration files, including environment variables, to streamline service updates and changes.
- **Automatic Logger Creation (not mandatory)**: Automatically creates dedicated loggers for each service, covering both classic and oneshot service types.
- **Help on I/O Redirection**: Provides keywords in frontend files for easy control over standard input, output, and error redirection.
- **Service Notification**: Ensures services are fully ready before managing their dependency chains, using a readiness notification mechanism.
- **Service Organization as a Tree**: Allows quick management and visualization of service groups within a tree structure.
- **Service Status Overview**: Offers a comprehensive set of tools to monitor the state of services and access detailed information easily.
- **User Service Declaration**: Users can declare and manage their own services, facilitating personalized service management.
- **Automatic Dependency Chains**: Automatically handles and maintains service dependencies, ensuring smooth and reliable service operations.
- **Service Order Dependencies**: Guarantees reliable, stable, and reproducible service order dependencies to maintain consistent service behavior.
- **Snapshot Management**: Allows the creation and management of snapshots of your service system, enabling easy backup, recovery, and sharing of service states across multiple hosts.

## Behavior Benefits:

- **No Reboot Required During Upgrades**: Service updates do not require system reboots, ensuring continuous operation.
- **Independent of Boot Management**: 66 can supervise services independently of the boot process, making it optional to use 66 from startup. It is also fully compatible with virtualization platforms like containerd, Podman, and Docker, allowing for easy monitoring of services within containers.
- **No Central Daemon**: Operates without a central managing daemon, providing a lightweight and efficient service management experience while reducing the potential attack surface.
- **Readable Logs**: Logs are stored in a human-readable format for easier analysis and debugging.
- **File Descriptor Holding for Log Pipes**: Utilizes file descriptor holding for efficient log piping, enhancing reliability and performance.

66 focuses on mechanisms, not policies, and can be compiled with either `glibc` or `musl` for flexibility across different systems.

**Note**: This documentation tries to be complete and self-contained. However, if you have never heard of [s6](https://skarnet.org/software/s6) you might be confused at first. Please refer to the skarnet documentation if in doubt.

## Installation

### Requirements

Please refer to the [INSTALL.md](https://git.obarun.org/Obarun/66/-/blob/master/INSTALL.md) file for details.

### Licensing

`66` is free software. It is available under the [ISC license](http://opensource.org/licenses/ISC).

### Upgrade

See [changes](66-upgrade.html) between version.

**(!)** The significant changes in versions `0.7.0.0` and above render them incompatible with versions prior to `0.7.0.0`. You can refer to the [Rosetta Stone](66-rosetta.html#changes-between-v0613-and-0700) to understand the interface and behavioral differences between versions below `0.7.0.0` and version `0.7.0.0`

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
