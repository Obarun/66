title: The 66 Suite: remove
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# remove

This command remove all components of a service. This operation **cannot be** undone. Process with caution.

## Interface

```
remove [ -h ] [ -P ] service...
```

This command remove all files belongs to *service* from the system even its log file. The only exception is its [frontend](66-frontend.html) file.

If the *service* is running, it will be stopped and unsupervised before removing it. This is also applied to its required-by dependencies except if the `-P` is passed.

In case of `module` *service* type, all *services* declared within the `module` will be removed. The `-P` has no effect on the *services* within the `module` and only affects the module's dependencies.

If the associated tree of *service* is a part of the group `boot`, the service is not stopped and not unsupervised. The changes will occur at the next boot.

This command handles [interdependencies](66.html#handling-dependencies).

## Options

- **-h** : prints this help.

- **-P**: Do not handle service dependencies. In such cases, the *remove* command will not attempt to remove the services that are dependent on the service.

## Usage example

Removes the service `foo`

```
66 remove foo
```

Removes the service `foo` without touching its interdependencies

```
66 remove -P foo
```