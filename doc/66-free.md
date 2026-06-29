# free

This command brings down *services* and removes them from the scandir.

## Interface

```
free [ -h ] [ -P ] service...
```

This command expects to find an already supervised service. If the state of the *service* is already up, the *free* command stop it first. Subsequently, service is unsupervised from the [scandir](66-scandir.html), freeing the memory used by the service.

Multiple *services* can be freed by separating their names with a space.

In case of `module` *service* type, all *services* declared within the `module` will also be unsupervised. The **-P** has no effect on the *services* within the `module` and only affects the module's dependencies.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h, --help**: prints this help.

- **-P, --no-propagate**: Do not handle service dependencies. In such cases, the *free* command will not attempt to stop the services that are dependent on the service, regardless of their current state.

## Usage examples

Free a service by handling its dependencies.

```
66 free foo
```

Free the service without handling any dependencies

```
66 free -P foo
```