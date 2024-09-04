title: The 66 Suite: snapshot
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# snapshot

This command handles snapshot of the 66 ecosystem of the owner of the process.

## Interface

```
snapshot [ -h ] create|restore|remove|list [<subcommand options>] name
```

This command creates, removes, restores or list snapshot of the 66 ecosystem for the owner of the current process depending on the provided options. This command requires a functioning 66 ecosystem and does not verify the presence of mandatory 66 directories or files. Therefore, avoid using this command during the initial installation of 66 without first running another 66 command.

**This command only handle 66 ecosystem for the owner of the process**. Making a snapshot as `root` do not handle users 66 ecosystem.

This command can be used at any time without impacting the state of the running services and may be usefull to downgrade a 66 ecosystem if a 66 automatic upgrade process go wrong whatever the reason.

This command can also usefull to transfert the exact same 66 ecosystem from a machine to another making a snapshot, copying it to the targeted machine and restoring it.

## Options

- **-h**: prints this help.

## Subcommands

- **create**: creates a snapshot of the 66 ecosystem.
- **restore**: restores a snapshot called *name*.
- **remove**: deletes a snapshot called *name*.
- **list**: list available snapshot.

## Usage examples

Creates a snapshot named foo

```
66 snapshot create foo
```

Restores a snapshot named foo

```
66 snapshot restore foo
```

Deletes a snapshot named foo

```
66 snapshot remove foo
```

List all available snapshot

```
66 snapshot list
```

### create

This subcommand creates a snapshot that doesn't exist yet.

#### Interface

```
snapshot create [ -h ] *name*
```

This subcommand make a snapshot called *named* containing a verbatim copy of the `%%system_dir%%/system`, `%%skel%%`, `%%service_admconf%%`, `%%environment_adm%%`, `%%seed_adm%%`, `%%service_adm%%`, `%%service_system%%`, `%%script_system%%` and `%%seed_system%%` for `root` user, and make a verbatim copy of the `$HOME/%%user_dir%%/system`, `$HOME/%%service_user%%`, `$HOME/%%script_user%%`, `$HOME/%%service_userconf%%`, `$HOME/%%seed_user%%` and `$HOME/%%environment_user%%` directory for regular users.

`%%livedir%%`, `%%system_log%%` and `$HOME/%%user_log%%` for regular users are not taken into account.

The prefix `system@` is reserved for snapshot names used in automatic upgrade processes. Please consider to choose a different prefix for your personal snapshot names.

#### Options

- **-h**: prints this help.

#### Usage examples

Creates a snapshot named `fooback`

```
66 snapshot create fooback
```

### restore

This subcommand restores a previously created snapshot.

#### Interface

```
snapshot restore [ -h ] *name*
```

This subcommand restores a previously created snapshot called *name*.

You can get a list of available snapshot invocating the [list](#list) subcommand.

The restoration process does not remove existing elements; instead, it overwrites any matching elements found in both the snapshot directory and the corresponding directory on the host

#### Options

- **-h**: prints this help.

#### Usage examples

Restores a snapshot named `fooback`

```
66 snapshot restore fooback
```

### remove

This subcommand removes a previously created snapshot.

#### Interface

```
snapshot remove [ -h ] *name*
```

This subcommand removes a previously created snapshot called *name*. This operation **cannot be** undone. Process with caution.

You can get a list of available snapshot invocating the [list](#list) subcommand.

#### Options

- **-h**: prints this help.

#### Usage examples

Removes a snapshot named `fooback`

```
66 snapshot remove fooback
```

### list

This subcommand list available snapshot.

#### Interface

```
snapshot list [ -h ]
```

This subcommand list all available snapshot for the owner of the current process.

#### Options

- **-h**: prints this help.

#### Usage examples

List available snapshot

```
66 snapshot list
```