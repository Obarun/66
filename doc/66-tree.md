title: The 66 Suite: 66-tree
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# 66-tree

This command handles a directory containing a set of *services*.

## Interface

```
    66-tree [ -h ] [ -z ] [ -v verbosity ] [ -l ] [ -n|R ] [ -a|d ] [ -c ] [ -S after_tree ] [ -E|D ] [ -C clone ] tree
```

*66-tree* will create, destroy, or modify a tree which dynamically handles *services*. *66-tree* will handle the tree only for the user running the process (root/user). Any *tree* is completely independent from another. If you want to know what trees are currently available on the system use the [66-intree](66-intree.html) tool.

## Exit codes

- *0* success
- *100* wrong usage
- *111* system call failed

## Options

- **-h** : prints this help.

- **-z** : use color.

- **-v** *verbosity* : increases/decreases the verbosity of the command.
    * *1* : only print error messages. This is the default.
    * *2* : also print warning messages.
    * *3* : also print tracing messages.
    * *4* : also print debugging messages.

- **-l** *live* : changes the supervision directory of *service* to *live*. By default this will be `%%livedir%%`. The default can also be changed at compile time by passing the `--livedir=live` option to `./configure`. An existing absolute path is expected and should be within a writable and executable filesystem - likely a RAM filesystem—see [66-scandir](66-scandir.html).

- **-n** : creates a new empty *tree*. The new *tree* ***must*** not exist on the system.

- **-a** *user* : allows *user* to deal with the given *tree*. ***Must*** match an existing username on the system. Several users can be allowed by separating their names with a comma. Any *user* not explicitly allowed is automatically denied for the given *tree*. By default the *user* issuing the command is automatically allowed when the tree is created.—This option sets the UID and GID of the service database at compilation time.

- **-d** *user* : denies *user* to deal with the specified *tree*. ***Must*** match an existing username on the system. Several users can be denied by separating their names with a comma.—Useful to revoke access that has been given before with **-a**.

- **-c** : makes tree the current *tree* to use by default with other *66* tools. The majority of commands in the *66* tools suite have a **-t** option to specify the *tree* to use. By setting *tree* as the current one all other *66* tools will use it by default. You can see which *tree* is the current one with the [66-intree](66-intree.html) tool.

- **-S** *after_tree* : Start the *tree* **after** *after_tree*. This tells [66-all](66-all.html) the specific order in which the start process is applied. *after_tree* and *tree* ***must*** be already enabled. You can combine this options directly with the **-E** option at *tree* creation time. **Note**: If *after_tree* and *tree* have the same name, this *tree* will be the very first tree to start.

- **-E** : enables *tree*. This allows the [66-all](66-all.html) tool to know which *tree* needs to be started. If the given *tree* is enabled, all *services* in that *tree* will be started when you use the [66-all](66-all.html) tool.

- **-D** : disables *tree*. The exact opposite of the **-E** option.

- **-R** : deletes *tree*. ***Can not be undone!*** This will completely remove the given *tree* from the system! You will not be able to retrieve any information of the deleted *tree* after deleting it. Services currently running on tree will be not bringed down before remove it. To do so, use the **-U** option in conjonction e.g. `66-tree -UR tree`.

- **-C** *clone* : makes a strict copy of *tree* named clone. Clone **must not** exist on the system.

## Why trees?

The usefulness of having several trees with different services can be explained with a simplified example.

```
    Tree1 contains dhcpcd & ntpd
    Tree2 contains cups & nfs
    Tree3 contains xorg, notification-daemon, gvfsd & dbus
```

When you boot your machine and want to use it from console only, you don't care about xorg or cups, you only care about a working internet connection. So at the base you only have Tree1 enabled. At every boot this tree and all its services will now be automatically started. Then you need to print something but for this, you also need to start the nfs daemon because your document is on some other server. Normally you would need to start cups then start nfs. Using the concept of trees you start Tree2 and everything is available. When you have finished printing your awesome document instead of stopping the needed services one by one you simply stop the whole Tree2 and all its containing services are stopped automatically. This doesn't stop here. Now you want to see a video, you need a running X server and probably several other services. Tree3 was designed just for that.

## Directories and files

***(!) Caution***: **AVOID** manipulating manually any directories and sub directories and their containing files of a *tree*. **CORRUPTION** and **ERRORS** may occur by doing so. The following details can help better understand the mechanics of the *66* tools. Nevertheless the *66* suite manages these paths dynamically and its tools like [66-enable](66-enable.html), [66-disable](66-disable.html), [66-start](66-start.html), [66-dbctl](66-dbctl.html) etc. will automatically search these directories to find information about a required *service*.

### At creation time

When creating a new *tree*, by default it will be created at `%%system_dir%%/system/tree` for root and at `$HOME/%%user_dir%%/system/tree` for any regular user.

The base name of the directory, namely `%%system_dir%%/` and `$HOME/%%user_dir%%/` can be changed at compile time by passing the `--with-system-dir=DIR` option for root and the `--with-user-dir=DIR` option for any regular user to `./configure`.

### When enabling a service

By enabling a *service* its corresponding [frontend](frontend.html) service file is parsed and the result written to a sub directory of the *tree*'s path called `servicedirs/svc` for `classic` services and `servicedirs/db/source` for `bundle` or `atomic` *services*.

As an example for root the resulting files would, by default, be found at `%%system_dir%%/system/tree/servicedirs/svc` for services of type `classic` and at `%%system_dir%%/system/tree/servicedirs/db/source` for any service of type `bundle` or `atomic`.

### When enabling a whole tree

When you ask to start all *services* of the currently enabled tree at once with the [66-all](66-all.html) tool, the directory `%%system_dir%%/system/tree/servicedirs/svc` is opened and the command will start any *service* found inside of that directory.

For *services* of type `bundle`,`module` and `atomic`, instead of opening the directory `%%system_dir%%/system/tree/servicedirs/db/source`, the corresponding compiled database found at `%%system_dir%%/system/tree/servicedirs/db/tree` is used. The database found at this location is the result of an automatic use of the command [s6-rc-compile](https://skarnet.org/software/s6-rc/s6-rc-compile.html) when enabling such a *service*.

