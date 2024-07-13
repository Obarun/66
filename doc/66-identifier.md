title: The 66 Suite: identifier
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# Identifier interpretation

The following table provides a comprehensive understanding of various identifiers used in the system. These identifiers are replaced with specific values before the parsing process to write generic frontend files. If an identifier is invalid, it will remain unchanged. In this context, the term `user` refers to the owner of the process.

| Identifier | Meaning | Replaced by |
| --- | --- | --- |
| `@I` | Instance name |	For instantiated services, this is the string between the first `@` character and the rest of the instantiated service name. For non-instantiated services, it is the service name. |
| `@U` | User name |	The user name. If the user is `0`, it is replaced by `root`. |
| `@u` | User UID |	The numeric user ID (`UID`). |
| `@G` | User group |	The user group name. |
| `@g` | User GID |	The numeric group ID (`GID`). |
| `@H` | User home directory | The user's home directory. For user `0`, it is `/root`. |
| `@S` | User shell | The user’s shell. |
| `@R` | User runtime directory | The user's runtime directory. For user `0`, it is `/run`; for other users, it corresponds to `$XDG_RUNTIME_DIR`. |

By understanding these identifiers and their replacements, you can effectively create and manage service files, ensuring that user-specific details are correctly populated during the service parsing process.

## Examples of identifiers usage

- Using `@R` identifier

    Original file: `dbus@`

    ```
    [Main]
    Type = classic
    Version = 0.7.0
    Description = "dbus session daemon for @U user"
    User = ( user )
    MaxDeath = 3
    Notify = 4
    TimeoutStart = 3000

    [Start]
    Execute = (
        /usr/bin/execl-cmdline -s { /usr/bin/dbus-daemon ${Args} }
    )

    [Stop]
    Execute = (
        /usr/bin/s6-rmrf ${Socket}
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
    MaxDeath = 3
    Notify = 4
    TimeoutStart = 3000

    [Start]
    Execute = (
        /usr/bin/execl-cmdline -s { /usr/bin/dbus-daemon ${Args} }
    )

    [Stop]
    Execute = (
        /usr/bin/s6-rmrf ${Socket}
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
