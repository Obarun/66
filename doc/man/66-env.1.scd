66-env(1)

# NAME

66-env - List or replaces an environment variables of a service depending
on the options passed.

# SYNOPSIS

66-env [ *-h* ] [ *-z* ] [ *-v* _verbosity_ ] [ *-t* _tree_ ] [ *-d* _dir_ ] [ *-L* ] [ *-r* _key=value_ ] _service_

# DESCRIPTION

*66-env* open and read the configuration file of _service_ find at
*%%service_admconf%%* by default.

It display the contain of the file or replace a _key=value_ pair if requested.

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

*-t* _tree_
	Specifies the _tree_ used to store the parsed _service_ file. This option
	is mandatory except if a tree was marked as 'current'. See *66-tree*(1).

	Ignored. Provides internal compatibility for for *66* tools.

*-d* _dir_
	Use _dir_ as configuration file directory instead of the default one.

*-L*
	Lists the environment variable of _service_.
	
*-e*
	Edit the configuration file with EDITOR set in your system environment.

*-r* _key=value_
	Override an existing _key=value_ pair with the given one. The
	_key=value_ needs to be single quoted in case of multiple arguments.

# EXIT STATUS

*0*		Success.++
*110*		Bad usage.++
*111*		system call failed.
	
# EXAMPLES

```
66-env -L ntpd
66-env -r CMD_ARGS=-d 
66-env -r CMD_ARGS='-c /etc/nginx/nginx.conf -g "daemon off;"'
```

# NOTES

Unexport a _key_ from the environment can be handled by using an exclamation
mark *!* at the begin of the _value_. For example:

	66-env -r CMD_ARGS='!-c /etc/nginx/nginx.conf -g "daemon off;"'
