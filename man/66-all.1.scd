66-all(1)

# NAME

66-all - Handle all _services_ for any _tree_ defined for the current user.

# SYNOPSIS

66-all [ *-h* ] [ *-v* _verbosity_ ] [ *-l* _live_ ] [ *-T* _timeout_ ] [ *-f* ] [ *-t* _tree_ ] *up* | *down*

# DESCRIPTION

Any enabled _tree_ (see *66-tree*(1)) or a specific one passed with the *-t* option,
and an already running _scandir_ will be processed.
It is a safe wrapper around *66-start(1)* and *66-stop*(1).

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.
	- *1* : (Default) Only print error messages.
	- *2* : Also print warning messages.
	- *3* : Also print debugging messages.

*-l* _live_
	Changes the supervision directory of _service_ to _live_. By default this will be
	*%%livedir%%*. An existing absolute path is expected and should be within a
	writable filesystem - likely a RAM filesystem. See *66-scandir*(1).

*-T* _timeout_
	Specifies a general timeout (in milliseconds) passed to *66-start*(1) or *66-stop*(1).
	By default the timeout is set to 0 (infinite).

*-f*
	Fork the process and lose the controlling terminal. This option should be
	used only for a shutdown process.

*-t* _tree_
	Only handles _service(s)_ for _tree_.

*up*
	Sends an up signal to every service inside any tree processed by the command.

*down*
	Sends a *down* signal to every _service_ inside any _tree_ processed by the command.

# INITIALIZATION PHASE

In case *up* was passed as option, *66-all* will launch *66-init*(1) to
initiate all _services_ of the _trees_ processed by the command. This means
either any enabled tree for the user currently launching the process or the
tree passed with the *-t* option.

# EXIT STATUS

*0*		Successful execution++
*111*		Failure

# SEE ALSO

*66-enable*(1) *66-init*(1) *66-start*(1) *66-stop*(1) *66-tree*(1)
