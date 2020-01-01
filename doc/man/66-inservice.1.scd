66-inservice(1)

# NAME

66-inservice - Display information about services.

# SYNOPSYS

66-inservice [ *-h* ] [ *-v* _verbosity_ ] [ *-c* ] -[ *-n* ] [ *-o* _name,intree,enabled,..._ ] [ *-g* ] [ *-d* _depth_ ] [ *-r* ] [ *-t* _tree_ ] [ *-p* _nline_ ] _service_

# DESCRIPTION

*66-inservice* displays detailed information about a specific _service_.

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	*1* : (Default) Only prints error messages.++
	*2* : Also prints warning messages.++
	*3* : Also prints tracing messages.++
	*4* : Also prints debugging messages.

*-c*
	enable colorization.

*-n*
	do not display the field name(s) specified.

*-o*
	comma separated list of fields to display. If this option is not passed,
	*66-inservice* will display all fields.

*-g*
	shows the dependency list of the _service_ as a hierarchical graph instead of a list.

*-d* _depth_
	Limits the depth of the dependency list visualisation; default is 1.
	This implies the *-g* option.

*-r*
	Shows the dependency list of _service_ in reverse mode.

*-t* _tree_
	only searches the _service_ at the specified _tree_, when the same 
	service may be enabled in more trees.

*-p* _nline_
	prints the _nline_ last lines from the log file of the _service_.
	Default is 20.

## VALID FIELD FOR -g OPTION

*name*
	Displays the name of the _service_.

*intree*
	Displays the _service_'s tree name.

*status*
	Displays the status.

*type*
	Displays the _service_ type.

*description*
	Displays the description of the _service_.

*intree*
	Displays the source of the _service_'s frontend file.

*live*
	Displays the live _service_'s directory.

*depends*
	Displays the _service_'s dependencies.

*optsdepends*
	Displays the _service_'s optional dependencies.

*extdepends*
	Displays the _service_'s external dependencies.

*start*
	Displays the _service_'s start script.

*stop*
	Displays the _service_'s stop script.

*envat*
	Displays the source of the environment file.

*envfile*
	Displays the contents of the environment file.

*logname*
	Displays the logger's name.

*logdst*
	Displays the logger's destination.

*logfile*
	Displays the contents of the log file.

# COMMAND AND OUTPUT EXAMPLE

The command "66-inservice 00" run as root user on _Obarun_'s default system
displays the following, where *00* is a service contained in the tree *boot*:

```
Name                  : 00
In tree               : boot
Status                : enabled, nothing to display
Type                  : bundle
Description           : Set the hostname and mount filesystem
Source                : %%service_packager%%/boot/mount/00
Live                  : %%livedir%%/tree/0/boot/servicedirs/00
Dependencies          : system-hostname  mount-run  populate-run  mount-tmp
                        populate-tmp  mount-proc  mount-sys  populate-sys
                        mount-dev  mount-pts  mount-shm  populate-dev
                        mount-cgroups
Optional dependencies : None
External dependencies : None
Start script          : None
Stop script           : None
Environment source    : None
Environment file      : None
Log name              : None
Log destination       : None
Log file              : None
    
```

By default the dependency graph is rendered in the order of execution. In 
this example the *oneshot system-hostname* is the first executed service and 
*mount-cgroups* is the last one when it finishes. You can reverse the rendered order with
the *-r* option.

You can display the status and 'depends-on' field and only these fields of a 
service using the command "66-inservice -o status,depends -g connmand"
where the dependency list is diplayed as a graph:

```
Status             : enabled, up (pid 938) 34652 seconds
Dependencies       : /
                     ├─(933,Enabled,longrun) dbus
                     └─(929,Enabled,longrun) connmand-log

```
You can display the status,log file and log destination and only these fields
of a service using the command "66-inservice -o status,logdst,logfile -g dbus":

```

Status             : enabled, up (pid 933) 34852 seconds, ready 34852 seconds
Log destination    : %%system_log%%/dbus
Log file           :
2019-10-03 16:16:05.246991500  dbus-daemon[933]: [system] Rejected send 
message, 4 matched rules; type="method_call", sender=":1.3" (uid=1000 
pid=1226 comm="connman-gtk --tray ") interface="net.connman.Technology" 
member="Scan" error name="(unset)" requested_reply="0" destination=":1.0"
(uid=0 pid=938 comm="connmand -n --nobacktrace --nodnsproxy ")
2019-10-03 16:16:35.256146500  dbus-daemon[933]: [system] Rejected send 
message, 4 matched rules; type="method_call", sender=":1.3" (uid=1000 
pid=1226 comm="connman-gtk --tray ") interface="net.connman.Technology" 
member="Scan" error name="(unset)" requested_reply="0" destination=":1.0"
(uid=0 pid=938 comm="connmand -n --nobacktrace --nodnsproxy ")

```
