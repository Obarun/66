![GitLabl Build Status](https://git.obarun.org/Obarun/66/badges/master/pipeline.svg)

# 66 - Service manager around S6 supervision suite

Sixty-six is service manager built around [s6](http://skarnet.org/software/s6) created to make the implementation and manipulation of service files on your machine easier.
It is meant to be a toolbox for the declaration, implementation and administration of services to achieve powerful functionality with small amounts of code.

Examples of what can be achieved by 66:
* Frontend service files declaration.
* Easy creation of a scandir.
* Nested supervision tree.
* Instance service file creation.
* Multiple directories service file declaration(packager,sysadmin,user).
* Easy change of service configuration.
* Automatic logger creation.
* Service Notification.
* Organizes services as a `tree`.
* Easy view of service status.
* User service declaration.
* Automatic dependencies service chain.
* ...

66 works on mechanisms not on policies. It can be compiled with `glibc` or `musl`.

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
  obarun@xmpp.obarun.org


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

This Roadmap for the next release is not writting in the stone. Feel free to make a merge request to this roadmap.

* [ ] Revise the frontend file's keyword field by excluding the `@` symbol:

  For instance, `@depends` will be `Depends`. That will allow for a file that's closer to the original INI format and less confusing for users.

* [ ] Provide a `[documentation]` section:

  Enable the provision of documentation for each service using a [documentation] section. This documentation will be easily accessible by invoking the 66 doc command.

* [ ] Provide a `Conflicts` keyword at frontend file:

  Allow to declare a conflicting service through the `Conflicts` field, e.g. `connmand` service will declare `Conflicts = ( Networkmanager )`.

* [ ] Provide keyword for basic operations:

  Certain repetitive tasks can be more efficiently managed directly by `66` in C rather than scripting them in the `Execute` field. For example, utilizing a `WorkDir` keyword can facilitate moving to the declared WorkDir value before executing the script.

* [ ] Reacts on event:

  Implementation of a daemon for event response to allow users to define services that dynamically start and stop when certain conditions are met, without needing to encode every possible condition in the service manager configuration.

* [ ] Ability to redirect stdin, stdout and stderr

  Allow to specify to make redirection of standard output

* [ ] Ability to Handle a general environment structure

  Every scandir will start with environment variable define by user through configuration file at specific directory, for instance `/etc/66/environment`.
  Ability through a new command to update this environment.