title: The 66 Suite: 66-start
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-start

This command starts one ore more *services* defined in *tree*.

## Interface

```
    66-start [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -r | R ] service(s)
```

This tool expects to find an already enabled service inside the given *tree* and an already running [scandir](66-scandir.html). If the state of the *service* is already up, *66-start* does nothing except when passing the **-r** or **-R** option—see [reload transition](66-start.html#Reload transitions).

Multiple *services* can be started by seperating their names with a space.

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

- **-T** *timeout* : specifies a timeout (in milliseconds) after which *66-start* will exit `111` with an error message if the *service* still hasn't reached the up state. By default, the timeout is `1000`.

- **-r** : If the *service* is up, restart it, by sending it a signal to kill it and letting s6-supervise start it again. By default, the signal is a `SIGTERM`; this can be configured via the `@down-signal` field in the [frontend service file](frontend.html).


## Dependencies handling

In case of `bundle`, `atomic` or `module` services, any dependency chain will be resolved automatically. It is unnecessary to manually define chained sets of dependencies. If FooA has a declared dependency on FooB, FooB will be automatically considered and started first when starting FooA. This will run recursively until all dependencies are started.

## Classic service transitions

- *66-start* gathers the classic service(s) passed as argument in a list called *selection*.

- The command first inspects this list to determine if any *service* in question was already written to it. If it was not then a new entry is added for each *service* passed as argument. It then copies the necessary files and directories of each newly written entry, creates a *down* file and the *event* fifo directory. After that it subscribes to the *services* fifo events, sends a reload signal to the scandir and waits for an event for every service in the *selection*. If a *timeout* occurs before receiving an event, the initialization fails.

- If a reload was forced the command will use a [reload](66-start.html#Reload transitions) transition instead.

- The *selection* is then inspected again and searched for any logger that needs to be associated with the passed *service*. If any such instruction was found the corresponding logger *service* will be added to the selection as well.

- Finally the command sends a `66-svctl -v verbosity -T timeout -l live -t tree -U selection` and waits for the resulting exit code.

If any one of these processes fails then as a result *66-start* fails too and exits with code `111`.

## Bundle, atomic and module transitions


- *66-start* gathers the `bundle`, `atomic` and `module` service(s) passed as argument in a list called *selection*.

- The command first inspects this list to determine if any service database in question of the given *tree* was already written to it. If it was not then a `66-init -v verbosity -l live -d tree` is automatically invoked and *66-start* will wait for the exit code of this automatic command. After that a reload signal is sent to the [scandir](66-scandir.html) to inform it about the change.

- If a reload was forced the command will use a [reload](66-start.html#Reload transitions) transition instead.

- Finally the command sends a `66-dbctl -v verbosity -T timeout -l live -t tree -u selection` and waits for the resulting exit code.

If any one of these processes fails then as a result *66-start* fails too and exits with code `111`.

## Reload transitions

### Classic services


- *66-start* gathers the classic service(s) passed as argument in a list called *selection*.

- If the service(s) require(s) a logger it will also be added to the *selection*. The command then sends a `66-svctl -v verbosity -T timeout -l live -t tree -D selection` and waits for the corresponding exit code. It removes all files and directories of the *service*(s) from the scandir and continues with an init transition—see above.

If this process fails then as a result *66-start* fails too and exits with code `111`.

### Bundle, atomic and module services

- *66-start* gathers the bundle and/or atomic service(s) passed as argument in a list called *selection*.

- The command first makes a backup of the services database live within the given tree and switches to the backup with the help of [s6-rc-update](https://skarnet.org/software/s6-rc/s6-rc-update.html).

- It then compiles the services database of the given *tree* from source with the help of [s6-rc-compile](https://skarnet.org/software/s6-rc/s6-rc-compile.html).

- Next it switches from the services database *live* of the given *tree* to this newly compiled database after which it then sends a `66-dbctl -v verbosity -T timeout -l live -t tree -d selection`.

- Finally it performs a `66-dbctl -v verbosity -T timeout -l live -t tree -u selection` and waits for the corresponding exit code.

If any one of these processes fails then as a result 66-start fails too and exits with code `111`.
