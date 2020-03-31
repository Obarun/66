66-start(1)

# NAME

66-start - Start one ore more _services_ defined in _tree_.

# SYNOPSYS

66-start [ *-h* ] [ *-z* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ *-t* _tree_ ] [ *-T* _timeout_ ] [ *-r* | *-R* ] _service..._

# DESCRIPTION

*66-start* expects to find an already enabled service inside the given _tree_
and an already running _scandir_. If the state of the _service_ is already up,
*66-start* does nothing except when passing the *-r* or *-R* option. See RELOAD
TRANSITIONS.

Multiple _services_ can be started by seperating their names with a space.

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
	Starts the _service_ from the given _tree_. This option is mandatory except
	if a tree was marked as _current_. See *66-tree*(1).

*-T* _timeout_
	Specifies a timeout (in milliseconds) after which *66-start* will exit 111
	with an error message if the service still hasn't reached the *up* state.
	By default, the timeout is *1000*.

*-r*
	If the service is up, restart it, by sending it a signal to kill it and
	letting s6-supervise start it again. By default, the signal is a SIGTERM;
	this can be configured via the *@down-signal* field in the frontend service
	file.

*-R*
	Reload the _service_ entirely.

# EXIT STATUS

*0*		Success.++
*111*		Failure.

# DEPENDENCIES HANDLING

In case of _bundle_ or _atomic_ services, any dependency chain will be resolved
automatically.
It is unnecessary to manually define chained sets of dependencies. If FooA has a
declared dependency on FooB, FooB will be automatically considered and started
first when starting FooA. This will run recursively until all dependencies are
started.

# CLASSIC SERVICE TRANSITIONS

*66-start* gathers the _classic_ service(s) passed as argument in a list called
_selection_.

The command first inspects this list to determine if any _service_ in question
was already written to it.
If it was not then a new entry is added for each service passed as argument.
It then copies the necessary files and directories of each newly written entry, 
creates a *down* file and the *event* fifo directory. After that it subscribes
to the _services_ fifo events, sends a reload signal to the *scandir* and waits
for an event for every service in the _selection_.
If a _timeout_ occurs before receiving an event, the initialization fails.

If a reload was forced the command will use a *reload* transition instead.

The _selection_ is then inspected again and searched for any logger that needs
to be associated with the passed _service_.
If any such instruction was found the corresponding logger service will be added to the
_selection_ as well.

Finally the command sends a "66-svctl -v _verbosity_ -T _timeout_ -l _live_ -t _tree_ -U _selection_"
and waits for the resulting exit code.

If any one of these processes fails then as a result *66-start* fails too and exits
with code 111.

# BUNDLE, ATOMIC TRANSITIONS

*66-start* gathers the _bundle_ and/or _atomic_ service(s) passed as argument in
a list called _selection_.

The command first inspects this list to determine if any service database in
question of the given _tree_ was already written to it. If it was not then a
"66-init -v _verbosity_ -l _live_ -d _tree_" is automatically invoked and
*66-start* will wait for the exit code of this automatic command. After that a
reload signal is sent to the _scandir_ to inform it about the change.

If a reload was forced the command will use a *reload* transition instead.

Finally the command sends a
"66-dbctl -v _verbosity_ -T _timeout_ -l _live_ -t _tree_ -u _selection_"
and waits for the resulting exit code.

If any one of these processes fails then as a result *66-start* fails too
and exits with code 111.

# RELOAD TRANSITIONS

## CLASSIC SERVICES

*66-start* gathers the classic service(s) passed as argument in a list called
_selection_.

If the _service(s)_ require(s) a logger it will also be added to the
_selection_. The command then sends a
"66-svctl -v _verbosity_ -T _timeout_ -l _live_ -t _tree_ -D _selection_"
and waits for the corresponding exit code. It removes all files and
directories of the _service(s)_ from the _scandir_ and continues with an
init transition.

If this process fails then as a result *66-start* fails too and exits with
code *111*.

## BUNDLE, ATOMIC SERVICES

*66-start* gathers the bundle and/or atomic service(s) passed as argument in a
list called *selection*.

The command first makes a backup of the services database _live_ within the
given _tree_ and switches to the backup with the help of *s6-rc-update*.

It then compiles the services database of the given _tree_ from source with the
help of *s6-rc-compile*.

Next it switches from the services database _live_ of the given _tree_ to this
newly compiled database after which it then sends a
"66-dbctl -v _verbosity_ -T _timeout_ -l _live_ -t _tree_ -d _selection_".

Finally it performs a
"66-dbctl -v _verbosity_ -T _timeout_ -l _live_ -t _tree_ -u _selection_"
and waits for the corresponding exit code.

If any one of these processes fails then as a result *66-start* fails too and
exits with code *111*.
