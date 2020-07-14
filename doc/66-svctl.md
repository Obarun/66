title: The 66 Suite: 66-svctl
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-svctl

Controls an already supervised *service* at live defined in a *tree*.

## Interface

```
    66-svctl [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -n death ] [ -u | d | r | K | X ] service(s)
```

This tool expects to find an already supervised *service* at *live* defined in the given *tree* and an already running [scandir](66-scandir.html).

***(!)*** This tool only deals with `classic` services—see [66‑dbctl](66-dbctl.html) for other types of services.

Multiple *services* can be handled by separating their names with a space. *66‑svctl* gathers each of these services in a list called *selection*. The command is sent to the *selection* asynchronously. 

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

- **-T** *timeout* : specify a timeout in milliseconds after which *66-svctl* will stop trying to reach the desired state of the *service*; defaults to `3000`. timeout is applied to each *service* of the *selection*. Separate timeouts for different services can be set in their respective [frontend](frontend.html) files using the `@timeout‑up` or `@timeout‑down` key. The [frontend](frontend.html) key is prioritized over this option.

- **-n** *death* : specifiy the maximum number of *service* death events that the supervisor will keep track of. If the *service* dies more than this number of times *66‑svctl* will stop trying to reach the desired state of the *service*. The [frontend](frontend.html) key `@maxdeath` is prioritized over this option—see [frontend](frontend.html). If this option is not specified and no max-death-tally file exists then the maximum allowed deaths will be `5` by default.

- **-u** : send an up signal to *service*. It searches for a notification-fd file—see [service startup notifications](https://skarnet.org/software/s6/notifywhenup.html) and [frontend](frontend.html). If this file exists *66‑svctl* will automatically change to uppercase `U`.

- **-d** : send a down signal to *service*. It searches for a notification‑fd file—see [service startup notifications](https://skarnet.org/software/s6/notifywhenup.html) and [frontend](frontend.html). If this file exists *66‑svctl* will automatically change to uppercase `D`.

- **-r** : send a reload signal to *service*. By default the signal is a `SIGTERM`; this can be configured with the `@down-signal` field in the [frontend](frontend.html) service file. It searches for a notification-fd file—see [service startup notifications](https://skarnet.org/software/s6/notifywhenup.html) and [frontend](frontend.html). If this file exists *66‑svctl* will automatically change to uppercase `R`.

- **-X** : exit; the supervisor of the *service* will exit too. This command should normally never be used on a working system.

- **-K** : send a SIGKILL to *service* and keep it down. 

## s6-svc corresponding signal

The *66‑svctl* signals correspond to [s6‑svc](https://skarnet.org/software/s6/s6-svc.html) in the following manner:

- *-u*  = -u
- *-U*  = -uwU
- *-d*  = -d
- *-D*  = -uwD
- *-r*  = -r
- *-X*  = -xd
- *-K*  = -kd

## Notes

You can also send a signal in a similar fashion to a `classic` or `longrun` service with the [s6‑svc](https://skarnet.org/software/s6/s6-svc.html)  tool which has the philosophy "launch and forget" which unlike *66‑svctl* does not care for the exit status of the service when a signal is sent.

As an extra convenience the **-n** option allows to deal directly with *max-death-tally* from the command line even if it was not specified explicitely in the services [frontend](frontend.html) file and no corresponding file exists.
