title: The 66 Suite: configure
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# configure

Handles an environment file and variable of a service depending on the options passed.

## Interface

```
configure [ -h ] [ -c version ] [ -s version ] [ -V|L ] [ -r key=value ] [ -i src,dst ] [ -e editor ] service
```

This command allows to handle the [configuration](66-service-configuration-file.html) file of *service* found by default at `%%service_admconf%%/<service>`. Depending of the options passed, you can displays the contents of the file, see the current version used, edits the configuration file and so on.

The edition of the [configuration](66-service-configuration-file.html) file is the **default** option if neither option is passed.

## Options

- **-h**: prints this help.

- **-c** *version*: makes *version* as the current one to use by default. If the service is already running, you need to restart the *service* to apply the change by the command e.g. `66 restart foo`.

- **-s** *version*: specifies the version to handles with the options **-V**, **-L**, **-r** and **-e** instead of using the current one.

- **-V**: lists available versioned configuration directories of the *service*. The term `current` is used to specific the current version in use as follow:

    ````
    0.2.0
    0.1.4 current
    ````
- **-r** *key=value*: override an existing `key=value` pair with the given one. The `key=value` needs to be single quoted in case of multiple arguments.This option can be specified multiple times.

- **-L**: lists defined environment variables for the *service*. It do the same for all files found at the configuration directory.

- **-i** *src,dst*: imports configuration file from *src* version to *dst* version. For instance, `0.3.1` is the *src* and `0.7.0` is the *dest*. The *src* version and *dst* version need to be separated by a comma without space before and after it. It **do not import** the configuration written by the parse process but only deal with extra configuration files written by the sysadmin.

- **-e** *editor*: edit the configuration file with *editor*. If you don't specify this option, it try to found the `EDITOR` variable from the environment variable. Note: the upstream file (meaning the one prefixed with a dot) is **never** touched. A copy of the upstream file is copied (if it doesn't exist yet) and the *configure* command  modifies that file.(see [Service configuration file](66-service-configuration-file.html) for further information).

## Usage examples

Lists the defined environment variable of the service `foo`

```
66 configure -L foo
```

Change the value of the `CMD_ARGS` variable for the service `nginx`

```
66 configure -r CMD_ARGS='-c /etc/nginx/nginx.conf -g "daemon off;"' nginx
```

Import the configuration file from the 0.1.6 version to th e0.1.7 version of the service `foo`

```
66 configure -i 0.1.6,0.1.7 foo
```

Get the list of version available for the service `foo`

```
66 configure -V foo
```

Edit the configuration file of the service `foo` using the `nano` editor

```
sudo 66 configure -e nano foo
```

See the the defined environment variable of the service `foo` from the `0.1.6` version

```
66 configure -s 0.1.6 -L foo
```

List the defined environment variables of the service `barz` of the module `foo@foobar`

```
66 configure -L foo@foobar:barz
```

## Notes

Removing a *key* from the environment variable during the execution time of the service can be handled by using an exclamation mark `!` at the begin of the value:

```
66 configure -r 'CMD_ARGS=!-c /etc/nginx/nginx.conf -g "daemon off;"' nginx
```

It this case the `CMD_ARGS` variable is not define at the environment variable of the service.