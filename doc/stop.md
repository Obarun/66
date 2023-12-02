title: The 66 Suite: stop
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# stop

This command stops one ore more *services*.

## Interface

```
stop [ -h ] [ -P ] service(s)
```

This command expects to find an already running [scandir](scandir.html) and an already parsed service. If the state of the *service* is already down, the *stop* command does nothing but handles required by dependency except when passing the **-P** option.

Multiple *services* can be stopped by seperating their names with a space.

The `66 -t` command option have no effect. `66` will detect automatically the associated *tree* of the service along its required by dependencies.

In case of `module` *service* type, all *services* declared within the `module` will stop. The **-P** has no effect on the *services* within the `module` and only affects the module's required by dependencies.

## Options

- **-h**: prints this help.

- **-P**: Do not handle services required by dependencies. In such cases, the *stop* command will not attempt to stop the services that depend on the service, regardless of their current state.

## Usage examples

Stop a service handling its required by dependencies.
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

You also can use the `-P` option to avoid handling the required by dependencies of the service inside the `module` service

```
66 stop -P foo@foobar:foobaz
```