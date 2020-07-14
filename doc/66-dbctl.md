title: The 66 Suite: 66-dbctl
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-dbctl

This command is used to control an already supervised *service* in *live* defined in *tree*.

## Interface

```
    66-dbctl [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -u | d | r ] service(s)
```
*66-dbctl* expects to find an already supervised *service* in *live* defined in the given *tree* and an already running *scandir*.

**(!)** This tool **only** deals with **'bundle'** and **'atomic'** services—for *'classic'* services see [66-svctl](66-svctl.html).

Multiple *services* can be handled by separating their names with a space. *66-dbctl* gathers the services passed as argument in a list called *selection*.

If *service* is not given, *66-dbctl* deals with all **'atomic'** services of the given *tree*.

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

- **-t** : handles the *selection* of the given *tree*. This option is mandatory except if a tree was marked as 'current'—see [66-tree](66-tree.html).

- **-T** *timeout* : specifies a general timeout (in milliseconds) after which *66-dbctl* will exit 111 with an error message if the *selection* still hasn't reached the desired state for each *service*; default is 0 (blocks indefinitely).

- **-u** : sends an *up* signal to the *service*.

- **-r** : reload the *service*. It sends a *down* signal then a *up* signal to the *service*.

## Note

This tool is a safe wrapper around [s6-rc](https://skarnet.org/software/s6-rc/s6-rc.html). It exclusively handles files that form part of the 66 ecosystem before sending the selection list to [s6-rc](https://skarnet.org/software/s6-rc/s6-rc.html).
