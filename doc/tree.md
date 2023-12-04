title: The 66 Suite: tree
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# tree

This command handles a *tree* containing a set of *services*.

## Interface

```
tree [ -h ] create|admin|remove|enable|disable|current|status|resolve|init|start|stop|free tree
```

The `tree` command functions similarly to services, wherein each *tree* can have dependencies or be required-by another *tree*. These subcommands facilitate various actions such as creating, managing, activating, and checking the status of trees, among other functionalities, within the system. Additionally, it manages dependencies between different trees, enabling effective control and organization of *tree* structures.

The default tree named `%%default_treename%%` is provided. This *tree* is automatically created using [basic creation configuration](#basic-creation-configuration), with the key difference that this *tree* is enabled by default. The default name can also be changed at compile time by passing the `--with-default-tree-name=` option to `configure`. If the `%%default_treename%%` is removed, a simple invocation of a `66` command will create it again.

Configuration of the *tree* during its creation can be managed through a configuration file called `seed`.—see [Seed files](#seed-files).

Services within *tree* need to be *enabled*, with the [66 enable](enable.html) command, to be managed by the `start` subcommand. The `stop` and `free` subcommand manages any running services within *tree*.

A non-existing *tree* can also be created automatically when invocating of the `66 -t` option with commands that accept it. For example, the `66 -t treefoo enable foo` call automatically creates the tree `treefoo` if it doesn't exist yet, applying the [basic creation configuration](#basic-creation-configuration) or utilizing the `seed` file configuration if such file exists.

## Why trees?

The usefulness of having several trees with different services can be explained with a simplified example:

```
tree named network contains services dhcpcd and ntpd
tree named print contains services cups and nfs
tree named graphic contains services xorg, notification-daemon, gvfsd and dbus
```

When booting your machine and opting for console-only usage, your concern might solely be a working internet connection, disregarding xorg or cups. Initially, you would only enable `network` at the base level. Upon every boot, this *tree* and all its *enabled* services will automatically start.

Later, when you need to print a document stored on another server, you'd typically start `cups` and then `nfs`. By leveraging the *tree* concept, you can start `print`, making all necessary services available. After finishing the printing task, instead of individually stopping the required services, you can simply stop `print`, and all services within it will cease automatically.

The functionality extends further: say, you now wish to watch a video, requiring an active X server and potentially other services. Enter `graphics`, specifically designed for such purposes.

## Options

- **-h**: prints this help.

## Subcommands

- **create**: creates a new *tree*.
- **admin**: manages *tree*.
- **remove**: removes *tree*
- **enable**: activates *tree* for the next boot.
- **disable**: deactivates *tree* for the next boot.
- **current**: mark *tree* as current.
- **status**: displays information about *tree*
- **resolve**: displays resolve file of *tree*
- **init**: initiate all services marked enabled from *tree*
- **start**: bring up all enabled services from *tree*
- **stop**: bring down all services from *tree*
- **free**: bring down and unsupervise all services from *tree*

### Usage examples

Create a non-existing *tree* named `network`
```
66 tree create network
```

Create a non-existing *tree* named `print` which depends on *tree* `network` and associated to the `admin` group
```
66 tree create -o depends=network:groups=admin print
```

Create a non-existing *tree* named `graphics` which depends on *tree* `network`
```
66 tree create -o depends=network graphics
```

Enables a *tree* called `network`
```
66 tree enable network
```

Starts the *tree* called `print`. Any *enabled* services associated with this *tree* are brought up.
```
66 tree start print
```

Stops the *tree* called `print`. Any running services will be stopped and unsupervised.
```
66 tree free print
```

### Create

This subcommand creates a *tree* that doesn't exist and potentially configures it based on the options used.

#### Interface

```
tree create [ -h ] [ -o depends=:requiredby=:... ] *tree*
```

After creation *tree* do not contain any services. You need to [associate](#associated-service-to-a-tree) services within *tree* with the [66 enable](enable.html) command.

#### Options

- **-h**: prints this help.

- **-o**: list of options separated by colons.

valid fields for `-o` options are:

   - **depends=**: comma separated list of dependencies for *tree* or `none`. If the name of the *tree* passed as dependencies doesn't exist, it will be automatically created.
   - **requiredby=**: comma separated list of trees required-by *tree* or none. If the name of the *tree* passed as required-by dependencies doesn't exist, it will be automatically created.
   - **groups=**: add *tree* to the specified groups. Accepted group are `boot`, `admin`, `user` and `none` —see [Groups behavior](#groups-behavior).
   - **allow=**: comma separated list of account to allow at *tree*. Account must be valid on the system. **name account is expected** not the corresponding `UID` of the account. The term `user` is also accepted to significate that all user neither root of the system can use *tree*.
   - **deny=**: comma separated list of account to deny at *tree*. Account must be valid on the system. **name account is expected** not the corresponding `UID` of the account. The term `user` is also accepted to significate that all user neither root of the system can use *tree*.
   - **clone=**: make a clone of *tree*. This create an exact copy of the configuration of the *tree*, with the exception that no services are associated with the cloned *tree*. The name of the clone **must not** already exists int the system.
   - **noseed**: do not use seed file to build the *tree*. Even if a seed file exists, ignore it and create tree only with options passed or [basic creation configuration](#basic-creation-configuration).

#### Usage examples

Creates a tree named `treefoo`
```
66 tree create treefoo
```

Creates, configures and clones a *tree* named `treefoo` where the clone of `treefoo` is named `treefoo2`
```
66 tree create -o depends=treebar,treebaz:groups=admin:deny=none:allow=root:clone=treefoo2 treefoo
```

### Admin

This command allow to manage configuration of an already existing *tree*.

#### Interface

```
tree admin [ -h ] [ -o depends=:requiredby=:... ] tree
```

If the *tree* does not exist, it is created and configured based on the options used.

#### Options

- **-h**: prints this help.

- **-o**: list of options separated by colons.

valid fields for `-o` options are:

   - **depends=** *trees*: comma separated list of *trees* dependencies for *tree* or `none`. If the name of the *tree* passed as dependencies doesn't exist, it will be automatically created.
   - **requiredby=**: comma separated list of *trees* required-by dependencies for *tree* or none. If the name of the *tree* passed as required-by dependencies doesn't exist, it will be automatically created.
   - **groups=** *group*: add *tree* to the specified *group*. Accepted group are `boot`, `admin`, `user` and `none` —see [Groups behavior](#groups-behavior).
   - **allow=** *user*: comma separated list of *user* account to allow at *tree*. Account must be valid on the system. **name account is expected** not the corresponding `UID` of the account. The term `user` is also accepted to significate that all user neither root of the system can use *tree*. Any user not explicitly allowed is automatically denied for configuring the given *tree*.
   - **deny=** *user*: comma separated list of *user* account to deny at *tree*. Account must be valid on the system. **name account is expected** not the corresponding `UID` of the account. The term `user` is also accepted to significate that all user neither root of the system can use *tree*.
   - **clone=** *name*: make a clone *name* of *tree*. This create an exact copy of the configuration of the *tree*, with the exception that no services are associated with the cloned *tree*. The name of the clone **must not** already exists int the system.

#### Usage examples

Changes the dependencies of `treefoo` to `treebaz` where `treefoo` depended previously of tree `treebar`
```
66 tree admin -o depends=treebaz treefoo
```

Deny all user of `treefoo`
```
66 tree admin -o deny=user treefoo
```

### Remove

This command remove a *tree*. This operation **cannot be** undone. Process with caution.

#### Interface

```
tree remove [ -h ] tree
```

Services associated with the tree are switched to the *tree* marked current or to the `%%default_treename%%` in other cases. The state of services remains unchanged during the switch.

Tree dependencies, including required-by dependencies, are managed. For instance, if `treefoo` is required-by `treebar`, the dependencies of `treebar` are adjusted, removing `treefoo` as a dependency. This adjustment is done recursively, ensuring all interdependencies are updated. Therefore, the entire tree dependency graph is reconstructed.

#### Options

- **-h**: prints this help.

#### Usage examples

```
66 tree remove treefoo
```

### Enable

This command enables a *tree* for the next boot.

#### Interface

```
tree enable [ -h ] tree
```

If the *tree* does not exist, it is created and configured with [basic creation configuration](#basic-creation-configuration).

Dependencies of trees are also managed in chains. For example, if the `treefoo` is enabled and depends on `treebar`', `treebar` will also be enabled. This process occurs recursively.

Trees associated to the `boot` [group](#groups-behavior) cannot be enabled.

#### Options

- **-h**: prints this help.

#### Usage examples

Enables `treefoo` tree
```
66 tree enable treefoo
```

### Disable

This command is the exact opposite of `enable` command.

#### Interface

```
tree disable [ -h ] tree
```

required-by dependencies of trees are also managed in chains. For example, if the `treefoo` is disabled and required-by `treebar`', `treebar` will also be disabled. This process occurs recursively.

#### Options

- **-h**: prints this help.

#### Usage examples

Disables `treefoo` tree
```
66 tree disable treefoo
```

### Current

This command a *tree* as the current one.

#### Interface

```
66 tree [ -h ] current tree
```

After marking a *tree* as current, the `66` command using the `-t` reacts to that *tree* without the need to specify the `-t` option.

#### Options

- **-h**: prints this help.

#### Usage examples

Marks `treefoo` as current
```
66 tree current treefoo
```

### Status

This command displays information about a *tree*.

#### Interface

```
tree status [ -h ] [ -n ] [ -o name,enabled,... ] [ -g ] [ -d depth ] [ -r ] tree
```

If *tree* is not specified, the command return information of all trees available of the system.

#### Options

- **-h**: prints this help.
- **-n**: do not display the names of fields. Combining this options with the `-o` facilitates scripting usage.
- **-o**: comma separated list of field to display.
- **-g**: displays the contents field as graph allowing to view interdependencies of services.
- **-d**: increase the depth view of the contents field recursion. Default value is `1`. Only have effect with the `-g` option.
- **-r**: reverse the contents field. By default, the order corresponds to a `start` process.

valid fields for `-o` options are:

   - **name**: displays the name of the tree.
   - **current**: displays a boolean value of the current state.
   - **enabled**: displays a boolean value of the enable state.
   - **depends**: displays the list of tree(s) started before.
   - **requiredby**: displays the list of tree(s) started after.
   - **allowed**: displays a list of allowed user to use the tree.
   - **contents**: displays the list of services associated to tree.

#### Usage and output examples

Displays all information of all trees
```
66 tree status
```

Only display the field `name` and `enabled` of all trees
```
66 tree status -o name,enabled
```

Only display the field `enabled` of tree `treefoo` without displaying the name of the field
```
66 tree status -n -o enabled treefoo
```

In a script you can do
```
#!/bin/sh

tree="${1}"
isenabled=$(66 tree status -no ${tree})

if [ ${isenabled} = "no" ]; then
   echo ${tree} is not enabled
else
   echo ${tree} is enabled
fi
```

Displays information of associated services of tree `global` using graph mode
```
66 tree status -g global

Name        : global
Current     : yes
Enabled     : yes
Allowed     : root
Groups      : admin
Depends     : \
              └─None
required by : \
              └─session (Enabled)
Contents    : \
              ├─dbus-log (pid=719, state=Enabled, type=classic, tree=global)
              ├─consolekit-log (pid=716, state=Enabled, type=classic, tree=global)
              ├─networkmanager-log (pid=715, state=Enabled, type=classic, tree=global)
              ├─sshd-log (pid=722, state=Enabled, type=classic, tree=global)
              ├─openntpd-log (pid=720, state=Enabled, type=classic, tree=global)
              ├─dbus (pid=728, state=Enabled, type=classic, tree=global)
              ├─sshd (pid=730, state=Enabled, type=classic, tree=global)
              ├─consolekit (pid=745, state=Enabled, type=classic, tree=global)
              ├─networkmanager (pid=746, state=Enabled, type=classic, tree=global)
              └─openntpd (pid=736, state=Enabled, type=classic, tree=global)
```
It also displays information of each associated services of tree `global` within () parentheses, which are self-explainatory.

Displays information about the associated services of the global tree in reverse graph mode, representing the process for stop
```
66 tree status -g -r global

Name        : global
Current     : yes
Enabled     : yes
Allowed     : root
Groups      : admin
Depends     : \
              └─None
required by : \
              └─session (Enabled)
Contents    : \
              ├─openntpd (pid=736, state=Enabled, type=classic, tree=global)
              ├─networkmanager (pid=746, state=Enabled, type=classic, tree=global)
              ├─consolekit (pid=745, state=Enabled, type=classic, tree=global)
              ├─sshd (pid=730, state=Enabled, type=classic, tree=global)
              ├─dbus (pid=728, state=Enabled, type=classic, tree=global)
              ├─openntpd-log (pid=720, state=Enabled, type=classic, tree=global)
              ├─sshd-log (pid=722, state=Enabled, type=classic, tree=global)
              ├─networkmanager-log (pid=715, state=Enabled, type=classic, tree=global)
              ├─consolekit-log (pid=716, state=Enabled, type=classic, tree=global)
              └─dbus-log (pid=719, state=Enabled, type=classic, tree=global)
```

### Resolve

This command displays the complete contents of the [resolve](deeper.html#resolve-files) file of the *tree*.

#### Interface

```
66 tree resolve [ -h ] tree
```

Resolve file are used internally by `66` to know *tree* information. This subcommand is purely a debug tool used by system administrator or developers in case of issues.

#### Options

- **-h**: prints this help.

#### Usage examples

Displays contents of the `treefoo` resolve file.
```
66 tree resolve treefoo
```
### Init

This command initiate services of a tree to a scandir directory

#### Interface

```
66 tree init [ -h ] tree
```

The behavior of this subcommand will depends of the state of the [scandir](scandir.html). If the scandir is not running, this command will initiate earlier services of *tree*, in other case its initiate all *enabled* services within *tree*.

This subcommand is primarily used internally by `66 boot` command to initiate earlier services of *tree*. Initiation of services is made automatically at each invocation of `66 start` or `66 tree start` command is services was not initiate previously.

#### Options

- **-h**: prints this help.

#### Usage examples

Initiates all *enabled* services of `treefoo` to a running scandir
```
66 tree init treefoo
```

### Start

This command starts all services **marked enabled** within tree

#### Interface

```
tree start [ -h ] [ -f ] tree
```

If *tree* is not specified, the command manages all *enabled* services within all *enabled* trees available of the system.

Dependencies of *tree* are also managed in chains. For example, if the `treefoo` tree is started and depends on `treebar`', `treebar` will be started first. This process occurs recursively.

#### Options

- **-h**: prints this help.

#### Usage examples

Starts all services marked enabled of all enabled trees of the system
```
66 tree start
```

Starts all services marked enabled of tree `treefoo`
```
66 tree start treefoo
```

### Stop

This command stops all **running services** within tree

#### Interface

```
66 tree stop [ -h ] [ -f ] tree
```

If *tree* is not specified, the command manages all services within all *enabled* trees available of the system.

required-by dependencies of *tree* are also managed in chains. For example, if the `treefoo` tree is stopped and required-by `treebar`', `treebar` will be stopped first. This process occurs recursively.

#### Options

- **-h**: prints this help.
- **-f**: fork the process and lose the controlling terminal. This option should be used only for a shutdown process.

#### Usage examples

Stops all services from all enabled trees available on the system
```
66 tree stop
```

Stops all services of tree `treefoo`
```
66 tree stop treefoo
```

### Free

This command stops and unsupervise tree.

#### Interface

```
66 tree free [ -h ] [ -f ] tree
```

If *tree* is not specified, the command manages all services within all *enabled* trees available of the system.

required-by dependencies of *tree* are also managed in chains. For example, if the `treefoo` tree is stopped and required-by `treebar`', `treebar` will be stopped first. This process occurs recursively.

#### Options

- **-h**: prints this help.
- **-f**: fork the process and lose the controlling terminal. This option should be used only for a shutdown process.

#### Usage examples

Stops and unsuperives all services from all enabled trees available on the system
```
66 tree stop
```

Stops and unsupervise all services of tree `treefoo`
```
66 tree stop treefoo
```

## Basic creation configuration

By default, *tree* is created with the following configuration which can be translated to [seed](#seed-files) file format:

```
depends = none
requiredby = none
enable = false
## Depending on the owner of the current process,
## it will be the name of the account system.
allow = root
current = false
## Depending on the owner of the current process,
## it will be *admin* for root or *user* for regular user.
groups = admin
```

## Seed files

A `seed` file can be provided to automatically configure the tree during its creation. This file is expected to be found at `%%seed_adm%%`, `%%seed_system%%` or `$HOME/%%seed_user%%`, depending of the owner of the process. These locations can also be changed at compile time by passing the `--with-sysadmin-seed=`, `--with-system-seed=` and `--with-user-seed=` options in `configure`, respectively.

The `seed` file name need to correspond to the name of the tree to be configured.

The following template is self-explained:
```
## An empty field is not allowed. If the key is define, the value must exist and valid.
## In other case, simply comment it.

## This file use the two following type format:
##   Boolean: use false or true. Default false for an absent key.
##   List: a comma separated string list.

## Type: List
## Corresponds to e.g. '66 tree admin -o depends=treebar treefoo' or '66 tree create -o depends=treebar treefoo' command
## You can use the term 'none'.

#depends = none

## Type: List
## Corresponds to e.g. '66 tree admin -o requiredby=treebar treefoo' or '66 tree create -o requiredby=treebar treefoo' command
## You can use the term 'none'

#requiredby = none

## Type: Boolean
## Corresponds to e.g. '66 tree enable treefoo' command

#enable = false

## Type: List
## Corresponds to e.g. '66 tree admin -o allow=root treefoo' or '66 tree create -o allow=root treefoo' command
## If you want to allow any regular account without specifying a particular account name,
## you can use the term 'user'. The account name must be valid on the system.

#allow = root

## Type: List
## Corresponds to e.g. '66 tree admin -o deny=root treefoo' or '66 tree create -o deny=root treefoo' command
## If you want to allow any regular account without specifying a particular account name,
## you can use the term 'user'. The account name must be valid on the system.

#deny = user

## Type: Boolean
## Corresponds to '66 tree current' command

#current = false

## Corresponds to e.g. '66 tree admin -o groups=boot treefoo' or '66 tree create -o groups=boot treefoo' command
## Can be one of the term: 'boot', 'admin', 'user' or 'none'.
## Only one group is allowed.

#groups = admin
```

This `template` example of `seed` file can be found at [contributions/seed](https://git.obarun.org/Obarun/66/-/blob/master/contributions/seed/template) directory of the `66` git project.

## Groups behavior

This feature is in progress. Currently, the only group that has an effect is the `boot` group. A tree set to the `boot` group **cannot be** *enabled*. Trees associated with the `boot` group are automatically managed by `66` during boot time. Enabling a *tree* within the `boot` group results in services within the *tree* starting twice, which is likely not the intended behavior.

The primary purpose and future goal of this feature are to manage *tree* permissions and provide the ability to handle multiple trees simultaneously. For instance, requesting to `start` the `admin` group will initiate all trees within this group.

**(!)** This option and its mechanisms can be subject to change in future releases of `66`.

## Associated service to a tree

Different manner to process can be used to associate a service to a specific *tree*. You can accomplish this using the following method where `foo` is the name of the service and `treefoo` the name of the *tree*:

- The `66 -t treefoo enable foo` command will associate `foo` to `treefoo`. Every call of this command will switch the service to specified *tree*, e.g. `66 -t %%default_treename%% enable foo` switch `foo` to `%%default_treename%%` whatever the previous location of `foo`.
- If the service **was never parsed** and you launch the command `66 -t treefoo start foo`, foo will be parsed first and associated to `treefoo`. In such case, `foo` is a part of `treefoo` but not *enabled*.
- Automatically at parse process setting the `@intree=treefoo` key through the [frontend](frontend.html) service file of the service. In such case, at very first use of the service on the system, the service is associated to `treefoo`.
