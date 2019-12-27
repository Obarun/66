66-dbctl(1)

# NAME

66-dbctl - Control an already supervised _service_ in _live_ defined in _tree_.

# SYNOPSIS

66-dbctl [ *-h* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ *-t* _tree_ ] [ *-T* _timeout_ ] [ *-u* | *d* | *r* ] _service(s)_

# DESCRIPTION

*66-dbctl* expects to find an already supervised _service_ in _live_ defined in
the given _tree_ and an already running _scandir_.
This tool *only* deals with _bundle_ and _atomic_ services; for _classic_
services see *66-svctl*(1).

Multiple _services_ can be handled by separating their names with a space.
*66-dbctl* gathers the services passed as argument in a list called _selection_.

If _service_ is not given, *66-dbctl* deals with all _atomic_ services of the
given _tree_.

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	- *1* : (Default) Only print error messages.++
	- *2* : Also print warning messages.++
	- *3* : Also print tracing messages.++
	- *4* : Also print debugging messages.

*-l* _live_
	Changes the supervision directory of _service_ to _live_. By default
	this will be *%%livedir%%*. The default can also be changed at
	compile time by passing the *--livedir=*_live_ option to *./configure*. An
	existing absolute path is expected and should be within a writable
	filesystem - likely a RAM filesystem. See *66-scandir*(1).

*-t* _tree_
	Handles the _selection_ of the given _tree_. This option is mandatory
	except if a tree was marked as 'current'. See *66-tree*(1).

*-T* _timeout_
	Specifies a general timeout (in milliseconds) after which *66-dbctl*
	will exit 111 with an error message if the _selection_ still hasn't
	reached the desired state for each _service_; default is 0 (blocks
	indefinitely).

*-u*
	Sends an *up* signal to the _service_.

*-d*
	Sends a *down* signal to the _service_.

*-r*
	Reload the _service_. It sends a *down* signal then a *up* signal to the
	_service_.

# NOTES

This tool is a safe wrapper around *s6-rc*. It exclusively handles files that
form part of the *66* ecosystem before sending the _selection_ list to *s6-rc*.

# SEE ALSO

*66-scandir*(1) *66-svctl*(1) *66-tree*(1)
