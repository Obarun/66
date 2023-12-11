title: The 66 Suite: signal
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# signal

Controls an already supervised *service*.

## Interface

```
signal [ -h ] [ -wu | -wU | -wd | -wD | -wr | -wR ] [ -abqHkti12pcysodDuUxOrP ] service...
```

This command expects to find an already supervised *service* and an already running [scandir](scandir.html).

This command is the heart of `66` concerning service state change. Every other `66` command that need to send a signal to a service pass through this command.

Multiple *services* can be handled by separating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h**: print this help.
- **-P**: Do not handle service dependencies. In such cases, the *signal* command will not attempt to send signal to the services that are dependent on the service, regardless of their current state.
- **-wu**: do not exit until the service is up.
- **-wU**: do not exit until the service is up and ready and has notified readiness.
- **-wd**: do not exit until the service is down.
- **-wD**: do not exit until the service is down and ready to be brought up and has notified readiness.
- **-wr**: do not exit until the service has been started or restarted.
- **-wR**: do not exit until the service has been started or restarted and has notified readiness.
- **-a**: send a SIGALRM signal.
- **-b**: send a SIGABRT signal.
- **-q**: send a SIGQUIT signal.
- **-H**: send a SIGHUP signal.
- **-k**: send a SIGKILL signal.
- **-t**: send a SIGTERM signal.
- **-i**: send a SIGINT signal.
- **-1**: send a SIGUSR1 signal.
- **-2**: send a SIGUSR2 signal.
- **-p**: send a SIGSTOP signal.
- **-c**: send a SIGCONT signal.
- **-y**: send a SIGWINCH signal.
- **-o**: once. Equivalent to '-uO'.
- **-d**: send a SIGTERM signal then a SIGCONT signal.
- **-D**: bring down service and avoid to be bring it up automatically.
- **-u**: bring up service.
- **-U**: bring up service and ensure that service can be restarted automatically.
- **-x**: bring down the service and propagate to its supervisor.
- **-O**: mark the service to run once at most.
- **-r**: restart service by sending it a signal(default SIGTERM).

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
66 -T 5000 -wU -u foo
```