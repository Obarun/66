# stop

This command stops one or more *services*.

## Interface

```
stop [ -h ] [ -P ] service...
```

This command expects to find an already running [scandir](66-scandir.html) and an already parsed service. If the state of the *service* is already down, the *stop* command does nothing but handles required-by dependency except when passing the `-P` option. A classic service already down receives the order again: its state does not change and no event is sent. A service that is not supervised is skipped, said at verbosity 2, while the services that depend on it follow the normal flow. A name with a frontend that was never parsed is refused with `100`: it has no place in the graph.

The way down of a service waits for its [`[Stop] Timeout`](66-frontend.html#timeout-stop); once it has passed, the supervisor sends `SIGKILL`. A service killed that way stays down and supervised and counts as stopped: what it depends on stops. [66 runstate](66-runstate.html) shows its `result` as `timeout-stop`, and the command exits with `111`, saying at verbosity 2:

```
unable to stop service: <name> -- killed at its stop timeout
```

When a service fails to stop, what it depends on is not stopped; the other services named go to the end, and the command exits with `111`.

A oneshot that is `done` runs its `[Stop]` script once. A daemon in a crash loop stops looping and stays down and supervised.

The logger of a service keeps running when its service stops.

Multiple *services* can be stopped by separating their names with a space.

The `66 -t` command option has no effect. `66` will detect automatically the associated *tree* of the service along its required-by dependencies.

In case of `module` *service* type, all *services* declared within the `module` will stop. The module is marked down first, then its members stop in reverse order. The **-P** has no effect on the *services* within the `module` and only affects the module's required-by dependencies.

If the *service* is a [reactor](66-event.html) (it carries an `[Event]` section), `stop` brings it down but **does not disarm** it: its rule stays registered at `66-eventd` and keeps reacting — a `Do = start` reactor comes back up on its next event. Use [66 free](66-free.html) to disarm a reactor. An armed event source (`Type = event`) stays armed and supervised as well.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h, --help**: prints this help.

- **-P, --no-propagate**: Do not handle services required-by dependencies. In such cases, the *stop* command will not attempt to stop the services that depend on the service, regardless of their current state.

## Usage examples

Stop a service handling its required-by dependencies

```
66 stop foo
```

Stop the service without handling any dependencies

```
66 stop -P foo
```

## Modules services

Services within a `module` can also be managed independently. If you need to stop a particular service inside the `module`, specify the name of the `module` service followed by a colon `:` and the name of the service within the `module`

```
66 stop foo@foobar:foobaz
```

where `foo@foobar` is the name of the `module` service and `foobaz` the name of the service inside the `module` service.

You also can use the `-P` option to avoid handling the required-by dependencies of the service inside the `module` service

```
66 stop -P foo@foobar:foobaz
```