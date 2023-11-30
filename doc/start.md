title: The 66 Suite: 66-start
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# start

This command starts one ore more *services*.

## Interface

```
start [ -h ] [ -P ] service(s)
```

This command expects to find an already running [scandir](scandir.html). If the state of the *service* is already up, the *start* command does nothing but handles dependency except when passing the **-P** option.

Multiple *services* can be started by seperating their names with a space.

If the *service* has never been parsed, the *start* command will parse it and associate the service with a specific *tree* if the `66 -t` option was used, or if the `frontend` file defines the `@intree` field, or if a `tree` was marked as current—refer to the [tree current](tree.html) subcommand. Otherwise, the *service* will be associated with the `%%default_treename%%`. This process is applied to all dependencies of the service.

If the *service* was already parsed, the `66 -t` command option have no effect.

In case of `module` *service* type, all *services* declared within the `module` will start. The **-P** has no effect on the *services* within the `module` and only affects the module's dependencies.

## Options

- **-h** : prints this help.

- **-P** : Do not handle service dependencies. In such cases, the *start* command will not attempt to start the services that are dependent on the service, regardless of their current state.

## Usage examples

Start a service handling its dependencies.
```
66 start foo
```

Start the service without handling any dependencies
```
66 start -P foo
```

Start a *service* in a specific *tree* if the service was never parsed
```
66 -t treeA start foo
```

## Dependencies handling

Any dependency chain will be resolved automatically. It is unnecessary to manually define chained sets of dependencies. If FooA has a declared dependency on FooB, FooB will be automatically considered and started first when starting FooA. This will run recursively until all dependencies are started.

## Modules services

Services within a `module` can also be managed independently. If you need to start a particular service inside the `module`, specify the name of the `module` service followed by a colon `:` and the name of the service within the `module`

```
66 start foo@foobar:foobaz
```

where `foo@foobar` is the name of the `module` service and `foobaz` the name of the service inside the `module` service.

You also can use the `-P` option to avoid handling the dependencies of the service inside the `module` service

```
66 start -P foo@foobar:foobaz
```