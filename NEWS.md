# Changelog for 66

## Upgrade/downgrade instructions

Upgrading/downgrading between versions may the need of the *66-update* tool use. In that case the
release note specifies it.
To do so, first upgrade/downgrade the 66 package. Then make a *dry-run* using the **-d**
option of the 66-update tool:

```
# 66-update -d
```

You **should not have any error messages** before going further. If you gets an
error message, please fix it first.
If you do not have any idea of what to do to fix the issue, reverse your previous
installation and consider to ask for help on the [66 mailing list](https://obarun.org/mailman/listinfo/66_obarun.org).

The *dry-run* option do not changes anything on your system, you need now
to really update your system:

```
# 66-update
```

This same process and command uses need to be done with your regular user.

See the *66-update* documentation page for further information.

---

# In 0.3.0.1

- Fix `@optsdepends` and `@extdepends` behavior:
    These two fields now respect correctly the start order of the service's dependencies.

- *66-inservice* change:
    The field `Optional dependencies` and `External dependencies` displays now the name of the tree where the service is currently enabled after the `:` colon mark if any:

    ````
    External dependencies : dbus-session@obarun:base gvfsd:desktop
    Optional dependencies : picom@obarun:desktop
    ````

---

# In 0.3.0.0

This is a ***Major release***, you need to update your *trees* with *66-update* tool.

- Bugs fix

- [skalibs](https://skarnet.org/software/skalibs) dependency bumped to **2.9.2.1**

- [exeline](https://skarnet.org/software/execline) dependency bumped to **2.6.0.1**

- [s6](https://skarnet.org/software/s6) dependency bumped to **2.9.1.0**

- [s6-rc](https://skarnet.org/software/s6-rc) dependency bumped to **0.5.1.2**

- [oblibs](https://framagit.org/oblibs) dependency bumped to **0.0.6.0**

- Add **-z** to all tools to enable colorization:
    
    - *66-in{service,tree}* and *66-update* tools change the *-c* option to *-z* option to be consistent between all tools.


- *66-start* and *66-stop* exit 0 instead of 110 if the service is not enabled.

- *Frontend* service file change:
    
    - The field `@type` accepts a new kind of service called *module*. A *module* can be considered as an instantiated service. It works the same way as a service *frontend* file but allows to configure a set of different kind of services before executing the enable process. Also, the set of the services can be configured with the conjunction of a script called `configure` which can be written on any language.
    
    - *module* also comes with a new section named `[regex]` which contains the following field:
        - `@configure`
        - `@directories`
        - `@files`
        - `@infiles`
    
        Please see the *frontend* documentation for futher information.
    
    -  Allow commenting out of a service inside *@contents*, *@depends*, *@optdepends*, *@extdepends* field with the `#` character. The service name **must** begin with a `#` character without any space between the `#` and the name of the service, so it can be ignored.
    
        ````
        @depends = ( foo #bar fooB )
        ````
    
    - Add `@version` field:
        This field is currently not mandatory to allow time to adapt the existing service files on your system, but it **will be mandatory** in a future release.

    - Comments in the `[environment]` section is now kept at parsing process and written to the final service configuration file. This is useful to explain the use of a variable without the need to look on the executable script.

- *66-in{service,tree}* display now *up*,*down* or *unintialized* on status and graph dependencies field for *oneshot*, *bundle*, *module* services. This allows us to know if e.g an *oneshot* service is currently *up* or *down*.

- *66-enable* now accepts the new option **-m**. This option reacts the same as the **-c** option for new *key=value* pair but overwrites the change of the admin on existing *key=value* pair. A *key=value* pair which doesn't exist on the frontend file remains untouched.

- *Oneshot* now accepts the option *log* at the field `@options`. This allows a *oneshot* to have its own logger. As any other service the log file can be seen with the *66-inservice* tool. The log destination can be controlled by the `@destination` field at the `[logger]` section. Default is set at compile time by the *--with-system-log* or *--with-user-log* flag.

- New debug tools:
    - *66-inresolve*: This tool allows to read the contents of the `resolve` file.
    - *66-instate*: This tool allows to read the contents of the `state` file.

- *--disable-s6-log-notification* was added on the *configure* script to disable logger's [readiness notification](https://skarnet.org/software/s6/notifywhenup.html). By default it use the file descriptor number 3.

- *--with-s6-log-timestamp* flag was added on the *configure* script to set the default output date format for a logger at the compile time. See `configure --help` command.

- *--with-system-module*, *--with-sysadmin-module*, *--with-user-module* flags was added on the *configure* script to set the default *system*, *sysadmin* and *user* module directory respectively. 

- *--with-system-script*, *--with-user-script* flags was added on the *configure* script to set the default *system* and *user* script directory respectively.

----

# In 0.2.5.2


- Fix build: Remove bytes.h oblibs header file


----

# In 0.2.5.1

- Bugs fix

- 66-tree -S options: if after_tree and tree have the same name, tree is considered as the very first tree to start.

- 66-unmountall: do not umount SS_LIVE

- Add SIGPWR control file at creation of .s6-svscan directory

-----

# In 0.2.5.0

    
- Bugs Fix: Bad memory allocation

- 66-tree: add -S option which allows to order the start tree process even after it creation.

----

# In 0.2.4.1
    
- Hot fix: fix html documentation

---

# In 0.2.4.0

- Bugs fix: Bad memory allocation.

- Enable again RB_DISABLE_CAD but don't crash if its not available and warn user
    
    - Add @optsdepends and @extdepends field at [main] section:
        
        - @optsdepends can be considere as: "enable one on this service or none"
        
        - @extdepends can be considere as: "enable the service if it is not already declared on a tree"
    
    - 66-in{tree,service}:
        
        - in case of empty value the tools return None
        
        - add -n option: this avoids to display the name of the field.
        
        - add field at 66-intree:
            
            - start: displays the list of tree(s) started before
            
            - allowed: displays a list of allowed user to use the tree
            
            - symlinks: displays the target of tree's symlinks
        
        - add field at 66-inservice:
            
            - optsdepends: displays the service's optional dependencies
            
            - extdepends: displays the service's external dependencies
    
    - 66-shutdown skeleton: be safier and check if options are past
    
    - New tool:
        
        - 66-update: The 66-update program makes a complete transition of trees and the live directory using a old 66 format (the one being replaced) with the new 66 format.

---

# In 0.2.3.2

- Fix bad annoucement at 66-svctl

- Warn in case of bad key at parenthesis parsing process

---
    
# In 0.2.3.1

- Bugs fix: bad allocation memory

- Makefile: remove creation of empty directories. 66-tree will check all directories and create it if it missing.

---
    
# In 0.2.3.0

- Adapt to oblibs 0.0.4.0

- Remove RB_DISABLE_CAD at 66-boot by default and allow to enable it with -c option. Starting on virtual system like lxc crash with this option. Well be safe on every case.

- Use of new function log_? familly from oblibs:
    
    - standardization of the exit code
    
    - standardization of the verbosity output
    
    - allow verbosity to 4 to display debug message

- Doc typo fix        

---
    
# In 0.2.2.2

- Adapt to skalibs 2.9.1.0

- Remove insta_? deprecated function

---
    
# In 0.2.2.1

- Typo fix at html documentation

- Fix 66-intree without options

---

# In 0.2.2.0

- Bugs fix: Always check the existing of the 66 heart directories

- 66-info is now deprecated and splitted into two differents API:
    
    - 66-intree give informations about tree
    
    - 66-inservice give informations about services

---

# In 0.2.1.2

- Hot fix: fix 66-info graph display.

---
    
# In 0.2.1.1

- Hot fix: fix the build of the service dependencies graph.

---

# In 0.2.1.0

- Bugs fix.

- A synchronization is now made on reboot even with force option.
   
- The field @name has no effects except for instantiated service but can be omitted. The name of the frontend file is took as name. In case of instantiated service, the field @name must contain the complete name of the frontend service file -- Refer to the frontend documentation page.

- New skeleton file: rc.shutdown.final. This skeleton file will be run at the very end of the shutdown procedure, after all processes have been killed and all filesystems have been unmounted, just before the system is rebooted or the power turned off. This script normally remains empty.
   
- Extra tools has removed and provided as an independent programs at ttps://framagit.org/Obarun/66-tools.git except for 66-echo and 66-unmountall program which are a dependent part of 66.

- The account to run the s6-log program at the associate service logger can be set at compilation time by the --with-s6-log-user=USER options. The default is root. Obviously, the @runas field at the [logger] section overwritte it. This options set the account to run the uncaught-logs too. Also, this option can be overwritten with the '-l' option at 66-boot invocation and the '-L' option at 66-scandir invocation. 

---
    
In 0.2.0.4

- Bugs fix on memory allocation

- Respect policies decision of user: 
    
    - Remove -m option from 66-boot on init file

- Add -d feature to 66-scandir 

---

# In 0.2.0.3

- 66-parser: Fix write of configuration file, add -c|C features 

---
	
# In 0.2.0.2

- Fix the written of user configuration file 

- Fix oneshot and bundle status check

- Add -l features to 66-tree, rewrite tree_unsupervise function

---
	
# In 0.2.0.0

- New tools:
    
    - 66-boot.
	
    - 66-scanctl.
	
    - 66-shutdown.
	
    - 66-shutdownd.
	
    - 66-hpr.
	
    - 66-env.
	
    - 66-parser.
	
    - 66-which.
	
    - 66-echo.
	
    - 66-unmountall.
    
    - execl-subuidgid.

- New @key field and change on frontend file:
	
    - [Logger] section accept a @depends field, @timestamp accept none as value, readiness notification is a default.
	
    - @hiercopy in [main] allow to copy any file or directory coming from the directory of the service.

- 66-envfile is now deprecated, use execl-envfile in replacement.

- 66-enable: add -F, -c, -C features.

- 66-svctl is now asynchrone to launch services.

- Man pages are available.

- Environment variables limitation: maximum 100 files by directory,	each file cannot contain more than 4096 bytes or 50 variables.

- Syntax to unexport variable with execl-envfile change: the exclamation mark '!' need to be placed at begin of value instead of key. 

- Bugs fix

---

# In 0.1.0.1

- Bugs fix

- Add 66-getenv,66-writenv and 66-gnwenv extra-tools

---
	
# In 0.1.0.0

- Logger for nested tree at scandir creation time is disabled.

- Pass graph function to new ss_resolve_graph_? function.

- Split resolve file and state flags.

- Add ss_state_? function.

- Really unsupervise rc service, add rc_manage, rc_unsupervise function.

- Fix -U option for 66-tree.

- Fix 66-info, add -c option.

---

# In 0.0.2.2

- Hot fix, do not stop an empty db.

---

In 0.0.2.1
	
- Hot fix, do not append inner bundle with empty word.

---
	
# In 0.0.2.0

- Bugs fix.

- Add 66-tree -U options to unsupervise a tree from a scandir.

- Rewrite 66-info to provide color and more informations.

- Empty database cannot be initialized.

- Respect /etc/66/sysadmin/service even for user.

---

# In 0.0.1.1

- Bugs fix at rc_init function.
