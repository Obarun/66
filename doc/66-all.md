title: The 66 Suite: 66-all
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-all

This command handles all *services* for any *tree* defined for the current user of the proccess at once.

## Interface

```
    66-all [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -T timeout ] [ -f ] [ -t tree ] up|down|unsupervise
```

Any enabled *tree*—see [66-tree -E](66-tree.html)—or a specific one passed with the **-t** option, and an already running *scandir* will be processed. It is a safe wrapper around [66-start](66-start.html) and [66-stop](66-stop.html).

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

- **-l** *live* : changes the supervision directory of *service* to *live*. By default this will be `%%livedir%%`. The default can also be changed at compile time by passing the `--livedir=live` option to `./configure`. An existing absolute path is expected and should be within a writable and executable filesystem - likely a RAM filesystem—see [66-scandir](66-scandir.html).

- **-T** *timeout* : specifies a general timeout (in milliseconds) passed to [66-start](66-start.html) or [66-stop](66-stop.html). By default the timeout is set to 0 (infinite).

- **-f** : fork the process and lose the controlling terminal. This option should be used only for a shutdown process.

- **-t** *tree* : only handles *service(s)* for *tree*.

- **up** : sends an *up* signal to every *service* inside any *tree* processed by the command.

- **down** : sends a *down* signal to every *service* inside any *tree* processed by the command.

- **unsupervise** : Bring down all *services* contained into the *tree* and remove the corresponding directory of the *service* from the [scandir](66-scandir.html).

## Initialization phase

In case *up* was passed as option, *66-all* will automatically launch [66-init](66-init.html) to initiate all *services* of the *trees* processed by the command. As described above this means either any enabled tree for the user currently launching the process or just the tree passed with the **-t** option.
