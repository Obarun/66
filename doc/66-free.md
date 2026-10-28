# free

This command brings down *services* and removes them from the scandir.

## Interface

```
free [ -h ] [ -P ] service...
```

This command expects to find an already supervised service. If the state of the *service* is already up, the *free* command stop it first. Subsequently, service is unsupervised from the [scandir](66-scandir.html), freeing the memory used by the service.

Multiple *services* can be freed by separating their names with a space.

In case of `module` *service* type, all *services* declared within the `module` will also be unsupervised. The **-P** has no effect on the *services* within the `module` and only affects the module's dependencies. A member declared with `Flags = ( earlier )` is left to the boot: it is neither stopped nor unsupervised. A service inside the module can be freed alone with its `module:service` name; the module is then untouched.

A logger is unsupervised after its service. A logger named is refused, its service named too or not: free its service instead.

Every stop of [66 stop](66-stop.html) applies, its stop delay and its exit code included. A service killed at its stop timeout is down: it is unsupervised with the others. A service that fails to stop stays supervised, with what it depends on; the other services are unsupervised, and the command exits with `111`. A name with a frontend that was never parsed is refused with `100`: it has no place in the graph.

If the *service* is a [reactor](66-event.html) (it carries an `[Event]` section), `free` is also what **disarms** it at `66-eventd`: unlike a plain [66 stop](66-stop.html), which leaves the rule reacting, `free` removes the reactor from the daemon so it reacts no more. For a `module`, every member reactor is disarmed in the same pass. An event source (`Type = event`) is disarmed the same way; freeing a `timer`, `inotify` or `schedule` source also disarms the reactors that depend on it or name it in their `From`, except under **-P**. Freeing the source of a `service` reactor leaves the reactor armed.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h, --help**: prints this help.

- **-P, --no-propagate**: Do not handle service dependencies. In such cases, the *free* command will not attempt to stop the services that are dependent on the service, regardless of their current state. The logger of a named service is not freed with it.

## Usage examples

Free a service by handling its dependencies.

```
66 free foo
```

Free the service without handling any dependencies

```
66 free -P foo
```