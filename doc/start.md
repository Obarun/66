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

If the *service* was already parsed, the `66 -t` command option have no effects.

In case of `module` *service* type, all *services* declared within the `module` will start. The **-P** has no effect on the *services* within the `module` and only affects the module's dependencies.

## Options

- **-h** : prints this help.

- **-P** : Do not handles services dependencies. In such case, the *start* command do not try to start the dependency of the service whatever the current state of the dependency.

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


