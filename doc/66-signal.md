# signal

Controls an already supervised *service*.

## Interface

```
signal [ -h ] [ -wu | -wU | -wd | -wD | -wr | -wR ] [ -abqHkti12pcyodDuUxOrP ] [ -s signal ] service...
```

This command expects to find an already supervised *service* and an already running [scandir](66-scandir.html).

This command is the heart of `66` concerning service state change. Every other `66` command that need to send a signal to a service pass through this command.

Multiple *services* can be handled by separating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies). The direction it takes depends on the operation asked for. The `-u`, `-U` and `-o` operations bring the service up, so they are propagated to its dependencies, which are brought up first. Every other operation is propagated to its required-by dependencies.

## Options

- **-h, --help**: print this help.
- **-P, --no-propagate**: Do not handle service dependencies. In such cases, the *signal* command only sends the signal to the services named on the command line, whichever direction the operation would otherwise propagate to.
- **-wu, --wait u**: do not exit until the service is up.
- **-wU, --wait U**: do not exit until the service is up and ready and has notified readiness.
- **-wd, --wait d**: do not exit until the service is down.
- **-wD, --wait D**: do not exit until the service is down and ready to be brought up and has notified readiness.
- **-wr, --wait r**: do not exit until the service has been started or restarted.
- **-wR, --wait R**: do not exit until the service has been started or restarted and has notified readiness.
- **-s, --signal** *signal*: send signal to the supervised process by signal name or its number.
- **-a, --alarm**: send a SIGALRM signal.
- **-b, --abort**: send a SIGABRT signal.
- **-q, --quit**: send a SIGQUIT signal.
- **-H, --hangup**: send a SIGHUP signal.
- **-k, --kill**: send a SIGKILL signal.
- **-t, --term**: send a SIGTERM signal.
- **-i, --interrupt**: send a SIGINT signal.
- **-1, --usr1**: send a SIGUSR1 signal.
- **-2, --usr2**: send a SIGUSR2 signal.
- **-p, --stop**: send a SIGSTOP signal.
- **-c, --cont**: send a SIGCONT signal.
- **-y, --winch**: send a SIGWINCH signal.
- **--stop-group**: send a SIGSTOP signal to the whole process group of the supervised process.
- **--cont-group**: send a SIGCONT signal to the whole process group of the supervised process.
- **--kill-group**: send a SIGKILL signal to the whole process group of the supervised process.
- **-o, --once**: once. Equivalent to '-uO'.
- **-d, --down**: send a SIGTERM signal then a SIGCONT signal.
- **-D, --down-keep**: bring down service and avoid to be bring it up automatically.
- **-u, --up**: bring up service.
- **-U, --up-restart**: bring up service and ensure that service can be restarted automatically.
- **-x, --exit**: bring down the service and propagate to its supervisor.
- **-O, --once-at-most**: mark the service to run once at most.
- **-r, --restart**: restart service by sending it a signal(default SIGTERM).

## Usage examples

Send a SIGHUP signal to `foo`

```
66 signal -H foo
```

Send a SIGHUP signal to `foo` by signal name

```
66 signal -s SIGHUP foo
```

Triggers a log rotation of `foo-log`

```
66 signal -a -P foo-log
```

Take down `foo` and block until the process is down and the finish script has completed

```
66 signal -wD -d foo
```

Bring up `foo` and block until it has sent notification that it is ready. Exit if it is still not ready after 5 seconds.

```
66 -T 5000 signal -wU -u foo
```