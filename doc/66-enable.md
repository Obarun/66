# enable

This command enables one or more services within a specified *tree* depending on the method used.

## Interface

```
enable [ -h ] [ -S ] [ -P ] service...
```

This command will also parse the service if it was never parsed, along with its `Depends` and the contents of a module. The *service* will then be available in the given *tree* for the next boot depending on the state of the *tree*. The targeted service(s) can also be started on the fly when enabling it with the **-S** option. Its dependencies and its logger are enabled too. A running service is not restarted.

The given *tree* can be define through the `-t` option in the `66` general option or by using the `InTree` field within the frontend file or setting a current tree through the [66 tree current](66-tree.html#current) subcommand. If neither `-t` options nor `InTree` is provided, or if no tree is marked as a current one, it will enable the service within the default tree named `%%default_treename%%`. The default tree name can be changed at compile time by passing the `-D default-tree-name` to `meson setup`. The `-t` option moves the named service and its logger to the given *tree*, whether it was already parsed or not; a dependency already parsed stays in its tree, one parsed now lands in the given *tree*.

In case of `module` service type, all services within the `module` are enabled. A service inside a module cannot be enabled alone: name the module instead.

If a *service* conflicts with an enabled one through its [Conflict](66-frontend.html#conflict) key, nothing is enabled. The rules that keep a provided name to a single service are described in [dependencies](66-dependencies.html#provide-i-also-answer-to-these-names).

With **-S**, enabling and starting are two distinct actions: when the start fails or is refused, the scandir not running included, the command exits with the code of [start](66-start.html) and the service stays enabled.

Multiple *services* can be enabled by separating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h, --help**: prints this help.

- **-S, --start**: starts the *service* on the fly directly after enabling it. If the state of the *service* is already up, this option will have no effect. This also applies to the dependencies of the service.

- **-P**: enable the named services only, not their dependencies nor their logger. A named service never parsed is parsed alone: its dependencies never parsed are not parsed. With **-S**, the named services only are started.

## Usage examples

Enable a service while specifying the tree name, moving it there if it was already parsed:

```
66 -t treeA enable foo
```

Enable an instanced service:

```
66 enable foo@foobar
```

Enable and start the service, also increase the default verbosity:

```
66 -v3 enable -S foo
```

Switch the provider of the name `network` from the enabled `connmand` to `networkmanager`, the services that depend on `network` staying enabled. The full sequence, with the stop of the service left and the hand over between a service and a provider of its name, is in [Provide](66-dependencies.html#provide-i-also-answer-to-these-names):

```
66 disable -P connmand
66 enable networkmanager
```


