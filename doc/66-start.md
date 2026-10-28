# start

This command starts one or more *services*.

## Interface

```
start [ -h ] [ -P ] service...
```

This command expects to find an already running [scandir](66-scandir.html). If the state of the *service* is already up, the *start* command does nothing but handles dependency except when passing the `-P` option. A classic service already up receives the order again and stays up: its state does not change, so no `up` event is sent and no reactor fires on it. A oneshot, a module or an event source already up is skipped, said at verbosity 2.

Multiple *services* can be started by separating their names with a space.

If the *service* has never been parsed, the *start* command will parse it and associate the service with a specific *tree* if the `66 -t` option was used, or if the `frontend` file defines the `InTree` field, or if a `tree` was marked as current—refer to the [tree current](66-tree.html#current) subcommand. Otherwise, the *service* will be associated with the `%%default_treename%%` tree. This process is applied to all dependencies of the service. A frontend the parse refuses starts nothing, see [parse](66-parse.html).

If the *service* was already parsed, the `66 -t` command option has no effect.

In case of `module` *service* type, all *services* declared within the `module` will start, in their order, and the module is marked up after them. The **-P** has no effect on the *services* within the `module` and only affects the module's dependencies.

A logger is started with its service. If a *service* conflicts with a running one through its [Conflict](66-frontend.html#conflict) key, nothing starts.

If the *service* is a [reactor](66-event.html) whose action is `Do = start` or `Do = restart`, it is armed: its rule is registered at `66-eventd` and its `Execute` does not run until its event comes. A reactor with another action starts as any service. The sources named in its `From` come up first, except under **-P**.

A service fails to start when its script fails or when it is not ready in time, and what waits for it is not started. The command then exits with `111` and says, at verbosity 2, how the service ended:

```
unable to start service: <name> -- exited with code <n>
unable to start service: <name> -- killed by signal <n>
unable to start service: <name> -- not ready within its start timeout
unable to start service: <name> -- a service it waits for failed
```

A classic service that declares [Notify](66-frontend.html#notify) has its [`[Start] Timeout`](66-frontend.html#timeout-start) to become ready, counted from the start of its process. When the time runs out, its supervisor brings it down, it stays down and supervised, and it is not started again until the next start.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h, --help**: prints this help.

- **-P, --no-propagate**: Do not handle service dependencies. In such cases, the *start* command will not attempt to start the services that the service depends on, regardless of their current state. The named services only are started, not their logger. A named service never parsed is parsed alone: its dependencies never parsed are not parsed, and stay absent until a command without **-P** parses them.

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