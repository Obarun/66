# remove

This command removes all components of a service. This operation **cannot be** undone. Proceed with caution.

## Interface

```
remove [ -h ] service...
```

This command removes all files belonging to *service* from the system even its log file. The only exception is its [frontend](66-frontend.html) file.

If the *service* is running, it will be stopped and unsupervised before removing it. The services that depend on it are stopped, unsupervised and removed with it, whether they are supervised or not. Nothing is written in the services it depends on: they simply no longer count it among the services that require them. When a service fails to stop, nothing is removed: what the command took out of the supervision is put back in the state it found it in, and the command exits with `111`.

A neighbour that names *service* in its [RequiredBy](66-frontend.html#requiredby) key keeps its declaration: once *service* is removed, the name is [absent](#absent-and-invalid-services) as long as a neighbour names it, and the removal says so at verbosity 2.

In case of `module` *service* type, all *services* declared within the `module` will be removed. A service inside a module cannot be removed alone: name the module instead.

A logger is removed with its service, its log directory included: `66 remove foo-log` is refused, `66 remove foo` removes both.

If the associated tree of *service* is a part of the group `boot`, the service is not stopped and not unsupervised. The changes will occur at the next boot. When the scandir is not running, the service is removed without being stopped.

`remove` takes a name literally: a name provided by another service through its [Provide](66-frontend.html#provide) key is refused, remove the provider by its own name instead. Removing a provider releases the names it answers to, nobody answers to them afterwards.

This command handles [interdependencies](66.html#handling-dependencies).

## Absent and invalid services

A name that a neighbour still names in its `RequiredBy` after its removal, or that was never parsed, is *absent*. `66 remove` of an absent name removes the services that depend on it, and the name stays absent as long as a neighbour names it.

`66 remove` accepts a named service that is invalid: a resolve file that does not read, a service on a dependency cycle, or a service whose frontend is gone from every directory 66 looks in. It is removed as far as its readable state allows; a running one whose resolve file does not read is stopped through its scandir entry. An invalid service met among the services the removal reaches, and not named, refuses the whole command with `100`.

## Options

- **-h, --help**: prints this help.

## Usage example

Removes the service `foo`

```
66 remove foo
```
