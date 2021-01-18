title: The 66 Suite: 66-stop
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# 66-stop

This command stops one ore more *services* defined in *tree*.

## Interface

```
    66-stop [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -u ] [ -X ] [ -K ] service(s)
```

The *service* to be stopped is expected to be enabled inside the given *tree* and started inside an already running [scandir](66-scandir.html). If the state of the service is already down *66-stop* does nothing. Generally speaking this command is the strict opposite of *66-start*.

Multiple *services* can be stopped by seperating their names with a space.

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

- **-T** *timeout* : specifies a timeout (in milliseconds) after which *66-stop* will exit `111` with an error message if the *service* still hasn't reached the up state. By default, the timeout is `1000`.

- **-u** : unsupervise the *service*. Remove the service directory from the [scandir](66-scandir.html) at the end of the stop process.

- **-X** : exit. The supervisor of the service will exit too. This command should normally never be used on a working system.

- **-K** : sends a `SIGKILL` to the service and keep it down.

## Dependencies handling

In case of `bundle`, `atomic` or `module` any existing dependency chain will be resolved automatically. It is unnecessary to manually define chained sets of dependencies. If FooA has a declared dependency on another service with the name FooB then FooB will be automatically considered and stopped first when stopping FooA. This works recursively until all dependencies are stopped.

## Classic service transitions


- *66-stop* gathers the classic *service*(s) passed as argument in a list called *selection*.

- The selection is then inspected and searched for any logger that may be associated with the passed *service*(s). If any such instruction was found the corresponding logger will be added to the *selection* as well.

- The command continues issueing `66-svctl -v verbosity -T timeout -l live -t tree -D selection` and waits for the resulting exit code.

- Finally the *service* directory is removed from the [scandir](66-scandir.html).

If any one of these processes fails then as a result *66-stop* fails too and exits with code `111`.

## Bundle, atomic and module transitions

The process for these *service* types is very similar to that of `classic` services except for the automated command that adapts accordingly.

- 66-stop gathers the `bundle`, `atomic` and `module` service(s) passed as argument in a list called *selection*.

- The selection is then inspected and searched for any logger that may be associated with the passed *service*(s). If any such instruction was found the corresponding logger will be added to the selection as well.

- Finally the command issues `66-dbctl -v verbosity -T timeout -l live -t tree -d selection` and waits for the resulting exit code.

If any one of these processes fails then as a result *66-stop* fails too and exits with code `111`.
