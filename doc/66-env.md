title: The 66 Suite: 66-env
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-env

Handles an environment file and variable of a service depending on the options passed.

## Interface

```
    66-env [ -h ] [ -z ] [ -v verbosity ] [ -t tree ] [ -c version ] [ -s version ] [ -V|L ] [ -r key=value ] [ -i src,dst ] [ -e ] service
```

- This program allows to handle the configuration file of *service* found by default at `%%service_admconf%%/<service>`. Depending of the options passed, you can displays the contents of the file, see the current version used, edits the configuration file and so on.

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

- **-c** *version* : makes *version* as the current one to use by default. If the service is already running, you need to restart the *service* to apply the change by the command e.g. `66-start -r <service>`. 

- **-s** *version* : specifies the version to handles with the options **-V**, **-L**, **-r** and **-e** instead of using the current one.

- **-V** : lists available versioned configuration directories of the *service*. The term `current` is used to specific the current version in use as follow:

    ````
        0.2.0
        0.1.4 current
    ````
- **-r** *key=value* : override an existing `key=value` pair with the given one. The `key=value` needs to be single quoted in case of multiple arguments.

- **-L** : lists defined environment variables for the *service*. It do the same for all files found at the configuration directory.

- **-i** *src,dst* : imports configuration file from *src* version to *dst* version. The *src* version and *dst* version need to be separated by a comma without space before and after it. It **do not import** the configuration written by the `66-enable` process but only deal with extra configuration files written by the sysadmin.

- **-e** : edit the configuration file with `EDITOR` set in your system environment. This is the default option if neither option is passed. If you use this option with a `sudo` command, you need to specify the `-E` option at sudo call—see [examples](66-env.html#Usage examples).

## Usage examples

```
    66-env -L ntpd

    66-env -r CMD_ARGS=-d 

    66-env -r CMD_ARGS='-c /etc/nginx/nginx.conf -g "daemon off;"'
    
    66-env -i 0.1.6,0.1.7 nginx
    
    66-env -V ntpd
    
    sudo -E 66-env -e ntpd
    
    66-env -s 0.1.6 ntpd
```

## Notes

Removing a *key* from the environment after the use can be handled by using an exclamation mark `!` at the begin of the value: 

```
    66-env -r CMD_ARGS='!-c /etc/nginx/nginx.conf -g "daemon off;"'
```
