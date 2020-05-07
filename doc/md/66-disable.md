title: The 66 Suite: 66-disable
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-disable

## Interface

```
    66-disable [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -S ] service(s)
```

This tool expects to find an already enabled *service* inside the given *tree*. The targeted service(s) can also be stopped on the fly when disabling it with the **-S** option. Generally speaking this command is the strict opposite of the [66-enable](66-enable.html) tool. 

Multiple *services* can be disabled by seperating their names with a space.

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

- **-S** : starts the *service* on the fly directly after enabling it. If the state of the *service* is already up, this option will have no effect unless the **-f** option is used to reload it.

## Dependencies handling

In case of `'bundle'` or `'atomic'` services, any dependency chain will be automatically resolved. It is unnecessary to manually define chained sets of dependencies. If FooA has a declared dependency on FooB, FooB will be automatically disabled as well when disabling FooA. This will run recursively until all dependencies are disabled.
