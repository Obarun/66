title: The 66 Suite: parse
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# parse

Parses a [frontend](frontend.html) service file and writes the result to a directory.

## Interface

```
parser [ -h ] [ -f ] [ -I ] service...
```

- Opens and reads the [frontend](frontend.html) *service* file.
- Runs a parser on the file.
- Writes the parsing result to the `%%system_dir%%/system/service/svc/foo` directory.

The absolute path of the frontend service file can also be set. In this case, the primary path of this absolute path must match `%%service_system%%` or `%%service_adm%%` or `$HOME/%%service_user%%` directory name e.g `%%service_system%%/nptd/0.1.1/nptd`.

## Options

- **-h**: prints this help.

- **-f**: force. Owerwrite an existing parsing result.

- **-I**: do not imports modified configuration files from the previous version used. Refer to [Service configuration file](service-configuration-file.html) for further information.

## Note

Users may not directly execute this command. It is primarily used internally by `66`. `66` automatically handles a non-parsed service by invoking the `parse` command. Users who wish to parse a service again with the `-f` option should prefer using the [reconfigure](reconfigure.html) command.

However, a system administrator might want to parse a service frontend file under construction to ensure everything is functioning correctly without altering the system's state.
