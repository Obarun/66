66-stop(1)

# NAME

66-stop - Stop one ore more _services_ defined in _tree_.

# SYNOPSYS

66-stop [ *-h* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ *-t* _tree_ ] [ *-T* _timeout_ ] [ *-u* ] [ *-X* ] [ *-K* ] _service..._

# DESCRIPTION

The _service_ to be stopped is expected to be enabled inside the given _tree_ and
started inside an already running _scandir_.
If the state of the _service_ is already down *66-stop* does nothing.
Generally speaking this command is the strict opposite of *66-start*(1).
Multiple _services_ can be stopped by seperating their names with a space.

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	*1* : (Default) Only print error messages.++
	*2* : Also print warning messages.++
	*3* : Also print debugging messages.

*-l* _live_
	Changes the supervision directory of _service_ to _live_. By default this
	will be *%%livedir%%*. The default can also be changed at compile time by
	passing the --livedir=_live_ option to *./configure*. An existing absolute
	path is expected and should be within a writable filesystem - likely a RAM
	filesystem. See *66-scandir*(1).

*-t* _tree_
	Stops _service_ in the given _tree_.
	This option is mandatory except if a tree was marked as _current_.
	See *66-tree*(1).

*-T* _timeout_
	Specifies a timeout (in milliseconds) after which *66-stop* will exit *111* with
	an error message if the service still hasn't reached the up state;
	defaults to *1000*.

*-u*
	Unsupervise the _service_.
	Remove the _service_ directory from the _scandir_ at the end of the stop process. This option only applies to services of type _classic_.

*-X*
	Exit. The supervisor of the service will exit too. This command should normally
	never be used on a working system.

*-K*
	Sends a SIGKILL to the _service_ and keep it down.

# EXIT STATUS

*0*		Success.
*111*		Failure.

# DEPENDENCY HANDLING

For _services_ of type _bundle_, _longrun_ or _oneshot_ any existing dependency
chain will be resolved automatically. It is unnecessary to manually define
chained sets of dependencies. If FooA has a declared dependency on another
service with the name FooB then FooB will be automatically considered and
stopped first when stopping FooA.
This works recursively until all dependencies are stopped.

# CLASSIC SERVICE TRANSITIONS

*66-stop* gathers the classic service(s) passed as argument in a list called
_selection_.

The _selection_ is then inspected and searched for any logger that may be
associated with the passed _service(s)_. If any such instruction was found the
corresponding logger will be added to the _selection_ as well.

The command continues issueing
"66-svctl -v _verbosity_ -T _timeout_ -l _live_ -t _tree_ -D _selection_"
and waits for the resulting exit code.

Finally the _service_ directory is removed from the _scandir_.

If any one of these processes fails then as a result *66-stop* fails too and exits with code *111*.

# BUNDLE, LONGRUN, ONESHOT TRANSITIONS

The process for these service types is very similar to that of classic services except for the automated command that adapts accordingly.

*66-stop* gathers the bundle, longrun and/or oneshot service(s) passed as
argument in a list called _selection_.

The _selection_ is then inspected and searched for any logger that may be
associated with the passed _service(s)_. 
If any such instruction was found the corresponding logger will be added to the _selection_ as well.

Finally the command issues
"66-dbctl -v _verbosity_ -T _timeout_ -l _live_ -t _tree_ -d _selection_" 
and waits for the resulting exit code.

If any one of these processes fails then as a result *66-stop* fails too and exits with code *111*.
