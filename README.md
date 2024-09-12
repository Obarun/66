![GitLabl Build Status](https://git.obarun.org/Obarun/66/badges/master/pipeline.svg)

# 66 - Service Manager Built Around the S6 Supervision Suite

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

## Installation

See the INSTALL.md file.

## Documentation

Online [documentation](https://web.obarun.org/software/66/)

## Contact information

* Email:
  Eric Vidal `<eric@obarun.org>`

* Mailing list
  https://obarun.org/mailman/listinfo/66_obarun.org/

* Web site:
  https://web.obarun.org/

* XMPP Channel:
  obarun@conference.xmpp.obarun.org


## Supports the project

Please consider to make [donation](https://web.obarun.org/index.php?id=18)

## Frontend service file

66 do not provide any frontend services file by default. 66 works on mechanisms not on policies.

### Boot service file

The boot sequence can be a tedious task to accomplish. A **portable** and **complete** set of services can be found [here](https://git.obarun.org/obmods/boot-66serv).
This set of service work out of the box and highly configurable to suit needs of the distributions.
POC was made on `Gentoo`, `Funtoo`, `Devuan`, `Void`, `Adelie`, `Antix`, `Arch` and `Obarun`.

### Runtime service file

You can find several examples for common daemon [here](https://git.obarun.org/pkg/observice) or [here](https://github.com/mobinmob/void-66-services)(thanks to mobinmob).

### Frontend service file scripting

By default, 66 use [execline](http://skarnet.org/software/execline) as scripting language. However, you can specify the scripting language to use.
[66-tools](https://git.obarun.org/obarun/66-tools) provide some additonal tools to help you on this task.
Some are specific to `execline` where other can be used on classic shell.

## Roadmap

This Roadmap for the next releases is not writting in the stone. Feel free to make a merge request to this roadmap.

* [x] Revise the frontend file's keyword field by excluding the `@` symbol:

  For instance, `@depends` will be `Depends`. That will allow for a file that's closer to the original INI format and less confusing for users.

* [ ] Provide a `[Documentation]` section:

  Enable the provision of documentation for each service using a [documentation] section. This documentation will be easily accessible by invoking the 66 doc command.

* [ ] Provide a `Conflicts` keyword at frontend file:

  Allow to declare a conflicting service through the `Conflicts` field, e.g. `connmand` service will declare `Conflicts = ( Networkmanager )`.

* [ ] Provide keyword for basic operations:

  Certain repetitive tasks can be more efficiently managed directly by `66` in C rather than scripting them in the `Execute` field. For example, utilizing a `WorkDir` keyword can facilitate moving to the declared WorkDir value before executing the script.

* [ ] Reacts on event:

  Implementation of a daemon for event response to allow users to define services that dynamically start and stop when certain conditions are met, without needing to encode every possible condition in the service manager configuration.

* [x] Ability to redirect stdin, stdout and stderr

  Allow to specify to make redirection of standard output

* [x] Ability to Handle a general environment structure

  Every scandir will start with environment variable define by user through configuration file at specific directory, for instance `/etc/66/environment`.

* [ ] Ability through a new command to update the general environment.