66-svctl(1)

# NAME

66-svctl - Control an already supervised _service_ at live defined in a tree

# SYNOPSYS

66-svctl [ *-h* ] [ *-z* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ *-t* _tree_ ] [ *-T* _timeout_ ] [ *-n* _death_ ] [ *-u* | *-d* | *-r* | *-K* | *-X* ] _service..._

# DESCRIPTION

*66-svctl* expects to find an already supervised _service_ at _live_ defined in
the given _tree_ and an already running _scandir_.

*66-svctl* only deals with _classic_ services. See *66-dbctl*(1) for other types of services.

Multiple _services_ can be handled by seperating their names with a space.
*66-svctl* gathers each of these services in a list called _selection_.
The command is sent to the _selection_ asynchronously.

# OPTIONS

*-h*
	Prints this help.

*-z*
	use color.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	*1* : (Default) Only print error messages.++
	*2* : Also print warning messages.++
	*3* : Also print tracing messages.++
	*4* : Also print debugging messages.

*-l* _live_
	Changes the supervision directory of _service_ to _live_. By default this
	will be *%%livedir%%*. The default can also be changed at compile time by
	passing the --livedir=_live_ option to *./configure*. An existing absolute
	path is expected and should be within a writable filesystem - likely a RAM
	filesystem. See *66-scandir*(1).

*-t* _tree_
	Handles a _service_ from the given _tree_. This option is mandatory
	except if a tree was marked as _current_. See *66-tree*(1).

*-T* _timeout_
	Specifies a timeout in milliseconds after which *66-svctl* will
	stop trying to reach the desired state of the _service_; defaults to 3000.
	_timeout_ is applied to each service of the selection. Separated timeouts
	for different services separately can be set in their respective _frontend_
	files using the *@timeout-up* or *@timeout-down* key. The frontend key is
	prioritized over this option.

*-n* _death_
	Specifies the maximum number of service _death_ events that the supervisor
	will keep track of. If the service dies more than this number of times,
	*66-svctl* will stop trying to reach the desired state of the services.
	The frontend key *@maxdeath* is prioritized over this option. See
	*66-frontend*(5). If this option is not specified and no _max-death-tally_
	file exists the maximum allowed deaths will be 5 by default.

*-u*
	Sends an *up* signal to _service_. It searches for a *notification-fd*
	file. See _service startup notifications and frontend_. If this file exist
	*66-svctl* will automatically change to uppercase *U*.

*-d*
	Sends a *down* signal to _service_. It searches for a *notification-fd*
	file. See _service startup notifications and frontend_. If this file exist
	*66-svctl* will automatically change to uppercase *D*.

*-r*
	Sends a *reload* signal to _service_. By default, the signal is a
	*SIGTERM*; this can be configured via the *@down-signal* field in the
	_frontend service_ file. It searches for a *notification-fd* file. See
	_service startup notifications and frontend_. If this file exist *66-svctl*
	will automatically change to uppercase *R*.

*-X*
	Exit. The supervisor of the service will exit too. This command should
	normally never be used on a working system.

*-K*
	Sends a *SIGKILL* to the _service_ and keep it down.

# EXIT STATUS

*0*		Success, the wanted state has been reached.++
*110*		Bad usage.++
*111*		System call failed.

# NOTES

From 66-svctl to s6-svc:++
*-u* -> *-u*++
*-U* -> *-uwU*++
*-d* -> *-d*++
*-D* -> *-uwD*++
*-r* -> *-r*++
*-X* -> *-xd*++
*-K* -> *-kd*

You can also send a signal in a similar fashion to a _classic_ or _longrun_ service with the
*s6-svc* tool which has the philosophy "launch and forget". In contrast to
*66-svctl* this does not care for the exit status of the service when a signal
is sent.

As an extra convenience the *-n* option allow you to deal directly on the
command line with _max-death-tally_ even if these were not specified
explicitely in the services frontend file and no corresponding files exist.
