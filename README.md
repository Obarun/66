![GitLabl Build Status](https://git.obarun.org/Obarun/66/badges/master/pipeline.svg)

66 - Service manager around S6 supervision suite
===

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

Installation
------------

See the INSTALL.md file.

Documentation
-------------

Online [documentation](https://web.obarun.org/software/66/)

Contact information
-------------------

* Email:
  Eric Vidal `<eric@obarun.org>`

* Mailing list
  https://obarun.org/mailman/listinfo/66_obarun.org/

* Web site:
  https://web.obarun.org/

* XMPP Channel:
  obarun@xmpp.obarun.org


Supports the project
---------------------

Please consider to make [donation](https://web.obarun.org/index.php?id=18)

Frontend service file
---------------------

66 do not provide any frontend services file by default. 66 works on mechanisms not on policies.

### Boot service file

The boot sequence can be a tedious task to accomplish. A **portable** and **complete** set of services can be found [here](https://git.obarun.org/obmods/boot-66serv).
This set of service work out of the box and highly configurable to suit needs of the distributions.
POC was made on `Gentoo`, `Funtoo`, `Devuan`, `Void`, `Adelie`, `Antix`, `Arch`.

### Runtime service file

You can find several examples for common daemon [here](https://git.obarun.org/pkg/observice) or [here](https://github.com/mobinmob/void-66-services)(thanks to mobinmob).

### Frontend service file scripting

By default, 66 use [execline](http://skarnet.org/software/execline) as scripting language. However, you can specify the scripting language to use.
[66-tools](https://git.obarun.org/obarun/66-tools) provide some additonal tools to help you on this task.
Some are specific to `execline` where other can be used on classic shell.
