title: The 66 Suite: init
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# init

This command initiates services from a given *tree*.

## Interface

```
init [ -h help ] tree
```

This command expects to find an already created [scandir](scandir.html) directory at `%%livedir%%/scandir/UID`, where *UID* is the uid of the current owner of the process.
Ther command `66 scandir create` need to executed before trying to run the `init` command. The [scandir](scandir.html) does not need to be necessarily running.

## Options

- **-h**: prints this help.

## Initialization process

The command will make an exact copy of the *enabled* service files and directories of the *tree* inside a [scandir](scandir.html) directory at `%%livedir%%/state/UID` where *UID* is the uid of the current owner of the process. The [scandir](scandir.html) does not need to be necessarily running. This is useful at boot time to initiate an early service before starting the scandir. Once the [scandir](scandir.html) starts—see [66 scandir start](scandir.html) command, the already present services start automatically.

If the [scandir](scandir.html) is running, you should invoke a [66 scandir reconfigure](scandur.html) command to inform it about the changes.

## Note

Users, even system administrator, should not need to directly invoke this command. It is primarily used internally by `66`. `66` automatically manages services that have not been initialized when necessary.