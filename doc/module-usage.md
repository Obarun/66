title: The 66 Suite: module service usage
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# Module service usage

This documentation describe how to use *module* from an user point of view. If you want to create a *module*, refers to [service module creation](module-creation.html) page.

A *module* is a [instantiated service](instantiated-service.html) and can be managed like any other instantiated services. For intance, to [start](66-start.html) it

```
66 start foo@bar
```

where `foo@` is the name of the *module* and `bar` the name of its intance. If the *module* was never parsed before, you will get the default configuration define by the developer of the *module* service.

Obviously you can [enable](66-enable.html) it

```
66 enable foo@bar
```

or makes these two operations in one pass

```
66 enable -S foo@bar
```

The advantage of a *module* reside in the facts that you can [configure](66-configure.html) the *module* according to your needs. To do so,

```
66 configure foo@bar
```

This command allow you to change the environment variable of the *module* and so, its configuration.

Now, you need to apply the changes

```
66 reconfigure foo@bar
```

The *module* should now use your configuration. The [reconfigure](66-reconfigure.html) command need to be called each time you change the configuration of the *module*. If you don't use the [reconfigure](66-reconfigure.html) command, the *module* keeps the previous configuration.

# Manage service within *module*

As *module* is a set of services, when you start it, several services may be started. You can get the list of the services within *module* using the [status](66-status.html) command.

```
66 status foo@bar
```

The command display different field notably the `contents` field corresponding to the list of the service within the *module*. You can control the state of these service like you do for any other services applying the following syntax

```
66 <command> <module_name>:<service_name>
```

For example, if `foo@bar` contain the service `baz`

```
66 stop foo@bar:baz
```

Simply separates the name of the *module* and the name of the service by a colon `:`.

A *module* can contain instantiated service. In this case use the same syntax specifying the complete name of the instantiated service. If `foo@bar` contain the instantiated service `bar@bou`, do

```
66 reload foo@bar:bar@bou
```