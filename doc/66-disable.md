# disable

This command disables one or more services.

## Interface

```
disable [ -h ] [ -S ] [ -P ] service...
```

This command expects to find an already enabled *service*. The targeted service(s) can also be stopped on the fly when disabling it with the **-S** option. Generally speaking this command is the strict opposite of the [enable](66-enable.html) command. A service already disabled, or never enabled, is left as it is; a running service keeps running unless **-S** is passed. A name with a frontend that was never parsed is refused with `100`.

The services that depend on the *service* are disabled too, each before its dependency, and a logger is disabled with its service.

Disabling a provider leaves the name it provides to it: it still answers to it. Disabling a provided name disables its provider.

The `66 -t` command option has no effect. `66` will detect automatically the associated *tree* of the service along its required-by dependencies.

In case of `module` service type, all services within the `module` are disabled. A service inside a module cannot be disabled alone: name the module instead.

Multiple *services* can be disabled by separating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h, --help**: prints this help.

- **-S, --stop**: stops the *service* on the fly directly after disabling it, and removes it from the scandir as [free](66-free.html) does. If the state of the *service* is already down, this option will have no effect. This also applies to the required-by  dependencies of the service. When the scandir is not running, the command exits with `100` and the service stays disabled.

- **-P**: disable the named services only, not the services that depend on them nor their logger. With **-S**, the named services only are freed.

## Usage examples

Disable the service

```
66 disable foo
```

Disable an instanced service

```
66 disable foo@foobar
```

Disable and stop the service, also increase the default verbosity:

```
66 -v3 disable -S foo
```

Switch the provider of the name `network` from the enabled `connmand` to `networkmanager`, the services that depend on `network` staying enabled. The full sequence, with the stop of the service left and the hand over between a service and a provider of its name, is in [Provide](66-dependencies.html#provide-i-also-answer-to-these-names):

```
66 disable -P connmand
66 enable networkmanager
```
