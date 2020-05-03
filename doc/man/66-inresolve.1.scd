66-inresolve(1)

# NAME

66-inresolve - displays the contents of the service's resolve file.

# SYNOPSYS

66-inresolve [ *-h* ] [ *-z* ] [ *-t* _tree_ ] [ *-l* ] _service_

# DESCRIPTION

*66-inresolve* displays the contents of the _service_'s resolve file.
This file are used internally by the 66 tools to know _service_ information. 
This tool is purely a debug tool used by developers.


# OPTIONS

*-h*
	Prints this help.

*-z*
	use color.

*-t* _tree_
	only searches the _service_ at the specified _tree_, when the same 
	_service_ may be enabled in more trees.

*-l*
	displays the contents of the associated logger's resolve file if any.
