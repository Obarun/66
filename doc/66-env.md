title: The 66 Suite: 66-env
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-env

Lists or replaces an environment variable of a service depending on the options passed.

## Interface

```
    66-env [ -h ] [ -z ] [ -v verbosity ] [ -t tree ] [ -d dir ] [ -L ] [ -e ] [ -r key=value ] service
```

- This program opens and reads the configuration file of *service* found by default at `%%service_admconf%%`

- It displays the contents of the file or replaces a `key=value` pair if instructed to do so with the **‑r** option.

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

- **-t** : handles the *selection* of the given *tree*. This option is mandatory except if a tree was marked as 'current'—see [66-tree](66-tree.html).

- **-d** *dir* : use *dir* as configuration file directory instead of the default one.

- **-L** : list defined environment variables for service.

- **-e** : edit the configuration file with `EDITOR` set in your system environment

- **-r** *key=value* : override an existing `key=value` pair with the given one. The `key=value` needs to be single quoted in case of multiple arguments.

## Usage examples

```
    66-env -L ntpd

    66-env -r CMD_ARGS=-d 

    66-env -r CMD_ARGS='-c /etc/nginx/nginx.conf -g "daemon off;"'
```

## Notes

Removing a *key* from the environment after the use can be handled by using an exclamation mark `!` at the begin of the value: 

```
    66-env -r CMD_ARGS='!-c /etc/nginx/nginx.conf -g "daemon off;"'
```
