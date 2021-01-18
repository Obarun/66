title: The 66 Suite: service configuration file
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# Service configuration file

If the `env` options at the `@options` field is set to a [frontend](frontend.html) service file, the `[environment]` section is copied to the `%%service_admconf%%/<service_name>/<service_version>/.service_name` file.

**Note**: The file name is prefixed with a dot.

This file is ***always*** written at [66-enable](66-enable.html) call. That means that every single changes made on this file will be lost. This is ensure you to you always have the environment variables set by upstream matching the needs of the service to be started properly.

However, to suit your needs you may want to change a value of a `key=value` pair or and a new one for a modified service. In that case, you need to make a copy of the upstream file to a new file at `%%service_admconf%%/<service_name>/<service_version>/` directories.

To accomplish this task, you have two solutions:

- use the [66-env](66-env.html) tool. This is the best and easy way to do it. This tool will take for you about the creation of the new file(if it doesn't exist yet) and allows you to edit it directly.
- copy manually the `%%service_admconf%%/<service_name>/<service_version>/.service_name` to `%%service_admconf%%/<service_name>/<service_version>/service_name`. The copied file can be named as you want.

At the service start process, the [execl-envfile](execl-envfile.html) tool will parses and reads every single file found at the `%%service_admconf%%/<service_name>/<service_version>/` directory in alphabetical order. That means that the upstream file will be read first than your modified file will be read. So, for a same `key=value` pair found at the upstream and modified file, the `key=value` pair coming from the modified file will be used.
