66-env(1)

# NAME

66-env - List or replace an environment variables of a service

# SYNOPSIS

66-env [ *-h* ] [ *-v* _verbosity_ ] [ *-t* _tree_ ] [ *-d* _dir_ ] [ *-L* ] [ *-r* _key=value_ ] _service_

# DESCRIPTION

*66-env* open and read the configuration file of _service_ find at
*%%service_sysconf%%* by default.

It display the contain of the file or replace a _key=value_ pair if requested.

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.++
	*1* : (Default) Only print error messages.++
	*2* : Also print warning messages.++
	*3* : Also print debugging messages.


*-t* _tree_
	Ignored. Provides internal compatibility for for *66* tools.

*-d* _dir_
	Use _dir_ as configuration file directory instead of the default one.

*-L*
	Lists the environment variable of _service_.

*-r* _key=value_
	Override an existing _key=value_ pair with the given one. The
	_key=value_ needs to be double-quoted in case of multiple arguments.
	Backslash *\\* should be used to properly parse a double-quoted element.

# EXIT STATUS

*0*		Success.++
*110*		Bad usage.++
*111*		system call failed.
	
# EXAMPLES

```
66-env -L ntpd
66-env -r CMD_ARGS=-d 
66-env -r "CMD_ARGS=-c /etc/nginx/nginx.conf -g \"daemon off;\""
```

# NOTES

Keys which are preceded by an exclamation mark *!* in the configuration
_service_ file can be accessed as if they had none. For example, a _key=value_
corresponding to:

	!CMD_ARGS=--print-pid=4

can be replaced by the command:

	66-env -r CMD_ARGS=--print-pid=3

In this case the operator will be kept even if it was not included in the
overwriting command.
