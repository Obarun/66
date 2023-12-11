title: The 66 Suite: restart
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# restart

This command restarts one or more services.

## Interface

```
restart [ -h ] [ -P ] service(s)
```

This command bring down and bring up again a *service*. This command expects to find an already running *service*.

The `66 -t` command option have no effect. `66` will detect automatically the associated *tree* of the service along its required-by dependencies.

In case of `module` service type, all services within the `module` are restarted.

Multiple *services* can be disabled by seperating their names with a space.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h**: prints this help.

- **-P**: Do not handle service dependencies. In such cases, the *restart* command will not attempt to restart the services that are dependent on the service, regardless of their current state.

## Usage examples

Restarts the `foo` service

```
66 restart foo
```

Restarts the `foo` service without handling its required-by dependencies

```
66 restart -P foo
```