66-parser(1)

# NAME

66-parser - Parse a _frontend_ service file and write the result to a directory

# SYNOPSIS

66-parser [ *-h* ] [ *-v* _verbosity_ ] [ *-f* ] [ *-c* | *-C* ] _service destination_

# DESCRIPTION

- Opens and reads the _frontend_ _service_ file.
- Runs a parser on the file.
- Writes the parsing result to the _destination_ directory.

An absolute path is expected for _service_ and _destination_.

# OPTIONS

*-h*
	Prints this help.

*-v* _verbosity_
	Increases/decreases the verbosity of the command.
	- *1* : (Default) Only print error messages.
	- *2* : Also print warning messages.
	- *3* : Also print tracing messages.
	- *4* : Also print debugging messages.

*-f*
	Force. Owerwrite an existing parsing result at _destination_.

*-c*
	merge it environment configuration file from frontend file.

*-C*
	overwrite it environment configuration file from frontend file.

# NOTES

*66-parser* will not try to read and parse any services declared under *@depends* 
key of the given frontend file. This tool is maily intended for debugging 
purpose and to see the result of a parsing process before actually enabling 
the service on the system. This tool use the exact same parser as *66-enable* 
which by default writes the configuration file to *%%service_sysconf%%*/_service_name_. 
That's mean that a corresponding file will be overwritten. To avoid this, 
it writes the configuration file at _destination/env/_ directory and adjust 
the resulting *run/finish* file to match the configuration file path.
