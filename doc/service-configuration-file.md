title: The 66 Suite: service configuration file
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# Service configuration file

If the an `[environment]` section is set at service [frontend](frontend.html) file, the `[environment]` section is copied to the `%%service_admconf%%/<service_name>/<service_version>/.service_name` file.

**Note**: The file name is prefixed with a dot.

This file is ***always*** written when the parser is called. That means that every single changes made on this file will be lost. This is ensure you to you always have the environment variables set by upstream matching the needs of the service to be started properly.

However, to suit your needs you may want to change a value of a `key=value` pair or and a new one for a modified service. In that case, you need to make a copy of the upstream file to a new file at `%%service_admconf%%/<service_name>/<service_version>/` directories.

To accomplish this task, you have two solutions:

- use the [configure](66-configure.html) command. This command should always be privilegied, this is the best and easy way to do it. This command create the new file if it doesn't exist and allows you to edit it directly.
- copy manually the `%%service_admconf%%/<service_name>/<service_version>/.service_name` to `%%service_admconf%%/<service_name>/<service_version>/service_name`. The copied file can be named as you want.

At the service start process, the [execl-envfile](execl-envfile.html) program will parses and reads every single file found at the `%%service_admconf%%/<service_name>/<service_version>/` directory in **alphabetical order**. That means that the upstream file will be read before any other file define within `%%service_admconf%%/<service_name>/<service_version>/` directory. At the end of the process, for a same `key=value` pair found at the upstream and modified file, the `key=value` pair coming from the modified file read will be used.
