title: The 66 Suite: instantiatied service
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# Instantiated service file creation

An *instantiated* service file is of the same syntax as decribed in the [frontend](frontend.html) document for any other service. It can be any *type* of service. However some differences exist :

- the name of the file needs to be appended with an `@` (commercial at) character.
- every value replaced in an instance file needs to be written with `@I`.

Example :

```
    File name : tty@

    Contents :

    [main]
    @type = classic
    @version = 0.0.1
    @description = "Launch @I"
    @user = ( root )

    [start]
    @execute = ( agetty -J 38400 @I )
```

By using [66 parse tty@tty1](parse.html) or any other `66` command, the resulting file will then be:

```
    [main]
    @type = classic
    @version = 0.0.1
    @description = "Launch tty1"
    @user = ( root )

    [start]
    @execute = ( agetty -J 38400 tty1 )
```
