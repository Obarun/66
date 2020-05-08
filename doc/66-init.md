title: The 66 Suite: 66-init
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-init

This command initiates services from a given *tree* at a *live* directory. 

## Interface

```
    66-init [ -h help ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] classic|database|both
```

This tool expects to find an already created [scandir](66-scandir.html) directory at *live* location. The [scandir](66-scandir.html) does not need to be necessarily running depending on the provided options. Administrators should invoke *66-init* only once.

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

## Arguments

- **classic** : only initiate all `classic` services from *tree* at *live* location. 

- **datatabe** : only initiate all `bundle` and `atomic` services from *tree* at *live* location. 

- **both** : initiate all `classic` services and all `bundle` and `atomic` services from *tree* at *live* location. 


## Initialization process

The tool will make an exact copy of all `classic` service files and directories of the *tree* inside a [scandir](66-scandir.html) directory at *live*. By default `%%livedir%%/scandir/UID` is created if it does not exist yet, where *UID* is the uid of the current owner of the process. The [scandir](66-scandir.html) does not need to be necessarily running. This is useful at boot time to initiate an early service before starting the scandir. Once the [scandir](66-scandir.html) starts—see [66-scandir -u](66-scandir.html) option, the already present services start automatically.

If the [scandir](66-scandir.html) is running, you should invoke a [66-scanctl reload](66-scanctl.html) command to inform it about the changes.

## Bundle, atomic services

The tool will automatically invoke [s6-rc-init -l live -c compiled -p prefix](https://skarnet.org/software/s6-rc/s6-rc-init.html), where, by default, *live* translates to `%%livedir%%/tree/UID/tree` and compiled to `%%system_dir%%/66/system/tree/servicedirs/db/tree` or `$HOME/%%user_dir%%/system/tree/servicedirs/db/tree` depending on the owner of the process and the *prefix* of the name of the *tree*. If it doesn't exist yet *live* is created in the process. For these services the [scandir](66-scandir.html) **must be** running. This tool like any other *66* tool can be invoked with user permissions.
