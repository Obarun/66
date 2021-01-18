title: The 66 Suite: 66-parser
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# 66-parser

Parses a [frontend](frontend.html) service file and writes the result to a directory.

## Interface

```
    66-parser [ -h ] [ -z ] [ -v verbosity ] [ -f ] [ -I ] service destination
```


- Opens and reads the [frontend](frontend.html) *service* file.
- Runs a parser on the file.
- Writes the parsing result to the *destination* directory.

An absolute path is expected for either *service* and *destination*.

## Exit codes

- *0* success
- *100* wrong usage
- *111* system call failed

## Options

- **-h** : prints this help.

- **-z** : use color.

- **-v** *verbosity* : increases/decreases the verbosity of the command.
    * *1* : only print error messages. This is the default.
    * *2* : also print warning messages.
    * *3* : also print tracing messages.
    * *4* : also print debugging messages.

- **-l** *live* : changes the supervision directory of *service* to *live*. By default this will be `%%livedir%%`. The default can also be changed at compile time by passing the `--livedir=live` option to `./configure`. An existing absolute path is expected and should be within a writable and executable filesystem - likely a RAM filesystem—see [66-scandir](66-scandir.html).

- **-f** : force. Owerwrite an existing parsing result at *destination*.

- **-I** : do not imports modified configuration files from the previous version used. Refer to [Service configuration file](service-configuration-file.html) for further information.

## Notes

*66-parser* will not try to read and parse any services declared under the `@contents`, `@depends`, `@optsdepends` or `@extdepends` key of the given [frontend](frontend.html) file even for a service `module` type. This tool is mainly intended for debugging purposes and to see the result of a parsing process before actually enabling the service on the system. The tool uses the exact same parser as [66‑enable](66-enable.html) which by default writes the configuration file to `%%service_admconf%%/service_name`. As a consequence any corresponding existing file will be overwritten. To avoid this, it writes the configuration file to the *destination/env/* directory and adjust the resulting *run/finish* file to match the configuration file path.
