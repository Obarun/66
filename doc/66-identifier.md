# Identifier interpretation

The following table provides a comprehensive understanding of various identifiers used in the system. If an identifier is invalid, it will remain unchanged. In this context, the term `user` refers to the owner of the process.

Identifiers are replaced at two distinct moments:

- **When parsing a frontend file** (`66 parse`, `66 enable`, ...), so that a single generic frontend produces a service tailored to its owner (and, for an instantiated service, to its instance).
- **At runtime, when an environment is loaded**, so owner-specific values are resolved on the running machine rather than baked in at parse time. This happens in two places:
    - When a scandir is brought up (`66 scandir start`, or the boot/user scandir), inside the files of the scandir environment directory (`/etc/66/environment` for `root`, `~/.66/environment` for a user), so the environment exported to the whole scandir carries owner-specific values.
    - When a service's environment file or directory is loaded (through `execl-envfile`) before the service is executed.

| Identifier | Meaning | Replaced by |
| --- | --- | --- |
| `@I` | Instance name |	For instantiated services, this is the string between the first `@` character and the rest of the instantiated service name. For non-instantiated services, it is the service name. **Parse time only** (see below). |
| `@U` | User name |	The user name. If the user is `0`, it is replaced by `root`. |
| `@u` | User UID |	The numeric user ID (`UID`). |
| `@G` | User group |	The user group name. |
| `@g` | User GID |	The numeric group ID (`GID`). |
| `@H` | User home directory | The user's home directory. For user `0`, it is `/root`. |
| `@S` | User shell | The user’s shell. |
| `@R` | User runtime directory | The user's runtime directory. For user `0`, it is `/run`; for other users, it corresponds to `$XDG_RUNTIME_DIR`. |

By understanding these identifiers and their replacements, you can effectively create and manage service files and scandir environment files, ensuring that owner-specific details are correctly populated.

## `@I` is parse time only

`@I` resolves an *instance* name, which only exists while parsing an instantiated frontend such as `tty@tty1`. There is no instance context at runtime, so **`@I` is never expanded at runtime**: it is left untouched in scandir environment files and in service environment files loaded by `execl-envfile`. Every other identifier is expanded in every context and resolves against the process owner.

## Examples of identifiers usage

- Using `@R` identifier

    Original file: `dbus@`

    ```
    [Main]
    Type = classic
    Version = 0.7.0
    Description = "dbus session daemon for @U user"
    User = ( user )

    [Start]
    Timeout = 3000
    Notify = 4
    MaxDeath = 3
    Execute = (
        /usr/bin/execl-cmdline -s { /usr/bin/dbus-daemon ${Args} }
    )

    [Stop]
    Execute = (
        /usr/bin/rm -f ${Socket}
    )

    [Environment]
    Args=!--session --print-pid=4 --nofork --nopidfile --address=unix:path=${Socket}
    Socket=!@R/bus
    ```

    Result after calling `66 parse dbus@oblive` where *oblive* has UID `1000`

    ```
    [Main]
    Type = classic
    Version = 0.7.0
    Description = "dbus session daemon for oblive user"
    User = ( user )

    [Start]
    Timeout = 3000
    Notify = 4
    MaxDeath = 3
    Execute = (
        /usr/bin/execl-cmdline -s { /usr/bin/dbus-daemon ${Args} }
    )

    [Stop]
    Execute = (
        /usr/bin/rm -f ${Socket}
    )

    [Environment]
    Args=!--session --print-pid=4 --nofork --nopidfile --address=unix:path=${Socket}
    Socket=!/run/user/1000/bus
    ```

    In this example, the `@R` identifier is replaced by `/run/user/1000`, updating the *Socket* field with the correct runtime directory for the user *oblive*. The `@U` identifier is replaced by *oblive* in the *Description* field.


- Using `@I` identifier

    Original file: `tty@`

    ```
    [Main]
    Type = classic
    Version = 0.0.1
    Description = "Launch @I"
    User = ( root )

    [Start]
    Execute = ( agetty -J 38400 @I )
    ```

    Result after calling `66 parse tty@tty1`

    ```
    [Main]
    Type = classic
    Version = 0.0.1
    Description = "Launch tty1"
    User = ( root )

    [Start]
    Execute = ( agetty -J 38400 tty1 )
    ```

    In this example, the `@I` identifier is replaced by `tty1`, resulting in the Description and Execute fields being updated accordingly.


- Using identifiers in the scandir environment

    A file `~/.66/environment/xdg` for the user *oblive* (UID `1000`):

    ```
    XDG_CACHE_HOME=@H/.cache
    XDG_CONFIG_HOME=@H/.config
    XDG_RUNTIME_DIR=@R
    ```

    Once the user scandir is brought up, the environment exported to the whole scandir becomes:

    ```
    XDG_CACHE_HOME=/home/oblive/.cache
    XDG_CONFIG_HOME=/home/oblive/.config
    XDG_RUNTIME_DIR=/run/user/1000
    ```

    Here `@H` is replaced by the owner's home directory and `@R` by the owner's runtime directory. An `@I` appearing in such a file would be kept as-is, since a scandir has no instance.
