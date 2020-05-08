title: The 66 Suite: 66-enable
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-enable

This command enables one ore more services inside a given *tree*.

## Interface

```
    66-enable [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -f|F ] [ -c|m|C ] [ -S ] service(s)
```

This tool expects to find a corresponding [frontend service file](frontend.html), a directory name (see [directory](#Directory name as service)) or a *service* instance (see [instance](#Instantiated service)), by default at `%%service_adm%%` or `%%service_system%%` in this order of precedence for root user and `$HOME/%%service_user%%`, `%%service_adm%%` or `%%service_system%%` in this order of precedence for a normal user. The default path can be changed at compile time by passing the `--with-system-service=DIR`, `--with-sysadmin-service=DIR` and `--with-user-service=DIR` to `./configure`. It will run a parser on the frontend service file and write the result to the directory of the given *tree*—see [66-tree](66-tree.html). The *service* will then be available in the given *tree* for the next boot depending on the state of the *tree*. The targeted service(s) can also be started on the fly when enabling it with the **-S** option.

Multiple *services* can be enabled by seperating their names with a space.

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

- **-l** *live* : changes the supervision directory of *service* to *live*. By default this will be `%%livedir%%`. The default can also be changed at compile time by passing the `--livedir=live` option to `./configure`. An existing absolute path is expected and should be within a writable filesystem - likely a RAM filesystem—see [66-scandir](66-scandir.html).

- **-t** : handles the *selection* of the given *tree*. This option is mandatory except if a tree was marked as 'current'—see [66-tree](66-tree.html).

- **-f** : reenables an already enabled *service* with the given options. This option will run again the process from the start and overwrite all existing files.

- **-F** : same as **-f** but also reenables its dependencies. 

- **-c** : only appends new `key=value` pairs to the environment configuration file of the *frontend* file.

- **-m** : appends new `key=value` and merges existing one to the environment configuration file from *frontend* file.

- **-C** : overwrites its environment configuration file from *frontend* file.

- **-S** : starts the *service* on the fly directly after enabling it. If the state of the *service* is already up, this option will have no effect unless the **-f** option is used to reload it.

## Dependency handling

For *services* of type `bundle` or `atomic` any existing dependency chain will be automatically resolved. It is unnecessary to manually define chained sets of dependencies. If FooA has a declared dependency on another service with the name FooB then FooB will be automatically enabled too when enabling FooA. This works recursively until all necessary dependencies are enabled.

## Directory name as service

When choosing to make a directory be recognised as service the path of the directory must exist by default at `%%service_adm%%`, `%%service_system%%` or `$HOME/%%service_user%%` depending of the owner of the process and the option given at compile time. All *service* files found in this directory will be enabled. This process is done recursively if a sub-directory is found till it not found other directories into the sub one. The directory can contain a mixed set of `bundle`, `atomic` services where some of those depend on each other. The directory option is not limited to these types though. Any available service type can be part of the set.

A good example is a set of services for the boot process. To achieve this specific task a large number of `oneshot` services is used along with some `classic` services.

The parser automatically resolves any existing dependency chain for the processed *services* just as it would for any regular service.

**(!)** This option and its mechanics can be subject to change in future releases of the *66-enable* tool.

## Instantiated service

An instanced *service* name from a service template can be passed as argument where the name of the *service* must end with a `@` (commercial at).—see [frontend service file](frontend.html).

**(!)** The name of the template must be declared first immediately followed by the instance name as shown in the following example:

```
66-enable tty@tty1
```

Also an instanced *service* can be declared on the `@depends` field of the [frontend service file](frontend.html).

## Service configuration file

If the [environment](frontend.html#Section: [environment]) section is set on the frontend service file, the parse result process can be found by default at `%%service_admconf%%` for the root user `$HOME/%%service_userconf%%` for a normal user. The default path can be changed at compile time by passing the `--with-sysadmin-service-conf=DIR` for the root user and `--with-user-service-conf=DIR` for a normal user.
