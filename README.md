![GitLabl Build Status](https://framagit.org/Obarun/66/badges/master/pipeline.svg)

66 - Helpers tools around S6 supervision suite
===

Sixty-six is a collection of system tools built around [s6](http://skarnet.org/software/s6) and [s6-rc](http://skarnet.org/software/s6-rc) created to make the implementation and manipulation of service files on your machine easier. 
It is meant to be a toolbox for the declaration, implementation and administration of services where seperate programs can be joined to achieve powerful functionality with small amounts of code.
	
Examples of what can be achieved by assembling different programs provided by 66:
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

* IRC Channel:
  Network: `chat.freenode.net`
  Channel : `#obarun`

Supports the project
---------------------

Please consider to make [donation](https://web.obarun.org/index.php?id=18)

Frontend service file
---------------------

66 do not provide any frontend services file by default. 66 works on mechanisms not on policies.

### Boot service file

The boot sequence can be a tedious task to accomplish. A **portable** and **complete** set of service can be found [here](https://framagit.org/pkg/obmods/boot-66serv).
This set of service work out of the box and highly configurable to suit needs of the distributions.
POC was made on `Gentoo`, `Funtoo`, `Devuan`, `Void`, `Adelie`, `Arch`.

### Runtime service file

You can find several examples for common daemon [here](https://framagit.org/pkg/observice)

### Frontend service file scripting

By default, 66 use [execline](http://skarnet.org/software/execline) as scripting language.
[66-tools](https://framagit.org/obarun/66-tools) provide some additonal tools to help you on this task.
Some are specific to `execline` where other can be used on classic shell.

Examples of common use
----------------------

Note: all 66 tools can be run as `root` or `regular user`.

Creates a new tree called `root`:

```
66-tree -n root
```

Enables the tree at boot time:
```
66-tree -E root
```

Enables a service inside a specific tree:
```
66-enable -t root dhcpcd tty@tty1 tty@tty2
```

Disables a service from a specific tree:
```
66-disable -t root dhcpcd
```

Starts a service:
```
66-start dhcpcd
```

Stops a service:
```
66-stop dhcpcd
```

View the status of a service:
```
66-inservice dhcpcd
```

View a specific fields of the service(name,status and the contain of the associated log file):
```
66-inservice -o name,status,logfile dhcpcd
```

View all services contained on a tree:
```
66-intree root
```

Starts all services in one pass for a specific tree:
```
66-all -t root up
```

Stops all services in one pass for a specific tree:
```
66-all -t root down
```
