title: The 66 Suite: 66-update
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-update

*66-update* is a program that should be used rarely. At upgrade/downgrade time, the inner file format used to retrieve informations of tree,service database currently in use, can differ between two versions. In this case, service databases and trees need to be rebuild to match the new format. The *66-update* program makes a complete transition of *trees* and the *live* directory using a old *66* format (the one being replaced) with the new 66 format.

The goal is to ensure a smooth transition between different versions of 66 without needing a reboot. 

## Interface

```
    66-udpate [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -d ] tree
```

If no *tree* is given, all trees of the owner (root or user) of the process will be processed. In the case of user owned trees *66-update* must run separately for each user. 

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

- **-d** : dry run. Performs operation without modify the current state of the system. All command's output will be prefixed by `dry run do:` sentence. It's a good idea to first use this option before performing a real update. 

## Transition process

- It saves the current state of the tree (current,enabled,...).

- It determines if the tree is already initialized. In such case, it makes a copy of the current database on a temporary directory and switches the database to it.

- It destroys the tree with `66-tree -R` command.

- It creates the tree with `66-tree -n(cEa)` command, reproducing the options of the saved state.

- It enables the service(s) previously declared on the tree.

- It switches the current database to the newly created database by the `enable` process.

At the end of the process, the *tree* and the *live* state has not changed. Services already in use are not stopped or restarted during the transition process and match exactly the same state before and after the process.

It tries to be safe as possible and exits `111` if a problem occurs. In any case on upgrade/downgrade time, *66* tools will not work properly if the transition is not made smoothly even by the sysadmin itself. So, we prefer to crash instead of leaving an inconsistent state and let the sysadmin correct the things. 
