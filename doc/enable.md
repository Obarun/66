title: The 66 Suite: enable
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# enable

This command enables one or more services within a specified *tree* depending of the method used.

## Interface

```
enable [ -h ] [ -S ] service...
```

This command will also parse the service if it was never parsed. The *service* will then be available in the given *tree* for the next boot depending on the state of the *tree*. The targeted service(s) can also be started on the fly when enabling it with the **-S** option.

The given *tree* can be define through the `-t` option in the `66` general option or by using the `@intree` field within the frontend file or setting a current tree through the [66 tree current](66-tree.html#current) subcommand. If neither `-t` options nor `@intree` is provided, or if no tree is marked as a current one, it will enable the service within the default tree named `%%default_treename%%`. The default tree name can be changed at compile time by passing the `--with-default-tree-name` to `./configure`.

In case of `module` service type, all services within the `module` are enabled.

Multiple *services* can be enabled by separating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h**: prints this help.

- **-S**: starts the *service* on the fly directly after enabling it. If the state of the *service* is already up, this option will have no effect. This also applies to the dependencies of the service.

## Usage examples

Enable a service while specifying with the tree name(this only takes effect if the service has not been parsed before):

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


