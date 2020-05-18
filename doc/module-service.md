title: The 66 Suite: module service creation
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# Module service creation

A module can be considered as an [instantiated](instantiated-service.html) service. It works the same way as a service frontend file but allows the user/admin to configure a set of different kind of services before executing the enable process. Also, the set of services can be configured with the conjunction of a script called *configure* which can be made in any language.

This allows one to pre-configure a set of services for special tasks without knowing the exact target of the module. The best example is the module for booting a machine. Each machine is different and the set of services needed to be adaptable as much as possible for different kinds of machines or boot; e.g booting a container.

A module is defined with two elements:

- an [instantiated frontend service](instantiated-service.html) file at `%%service_system%%` 

- a directory at `%%service_module%%`. 

The name of the frontend and the name of the directory **must** be the same. For example if the frontend is named `%%service_system%%/foo@`, the directory of the module must be `%%service_module%%/foo@`.

## Frontend file

The [instantiated frontend service](instantiated-service.html) is written as any other instantiated service with it own specification—see the section [[regex]](frontend.html#Section: [regex]).

## Module directory

The module directory can contain three sub-direcotories:

- *configure* : This directory can contain an **executable** file script named configure. For example, `%%service_module%%/foo@/configure/configure`. The sub-directory **must** be named *configure* and the file script **must** be named *configure*. This file **is not** mandatory. The [66-enable](66-enable.html) process will detect if the file exist and it run it if it's the case. It's up to you to write the configure script file with the language of your choice as long as you define a correct *shebang*.

	Also, this directory can contain any files or directories that you need to configure your module. It's the responsability of the module creator to properly use or dispatch files or directories found inside the *configure* directory with the help of the *configure* script or at module installation phase. [66-enable](66-enable.html) will not handle any other file than the *configure* script for you. 

	**Note** : The *configure* script is launched after the parsing of the frontend file, meaning all regex operations on directories and files are already made.

- *service* : This directory can contain any frontend files for any kind of service ***except*** instantiated frontend service files. Also, this directory can contain sub-directories containing another frontend service files. This can be done recursively.

- *service@* : This directory can contain any instantiated frontend service files and **only** instantiated frontend service files. Also, this directory can contain sub-directories containing other instantiated frontend service files. This can be done recursively.

Any services that you need **must** be present inside the *service* or *service@* directory. [66-enable](66-enable.html) only deals with these directories. If a service `fooA` has `fooB` as dependency, `fooA` ***and*** `fooB` **must** exist in the `%%service_module%%/<module_name>/service` directory.

## A word about the [[regex]](frontend.html#Section: [regex]) section `@addservices` field

You may need an existing frontend service file from your system to configure your module. Instead of redefining this frontend file, use this field. The [66-enable](66-enable.html) process will verbatim copy the frontend service file inside `%%service_system%%/<module_name>/service` or `%%service_system%%/<module_name>/service@` module directory, depending on the type of service. This copying is made **after** all regex operations, and does not modify the frontend file at all.

## A word about the [[main]](frontend.html#Section: [main]) section with the module type

The valid fields in section [[main]](frontend.html#Section: [main]) are:

- @type
- @description
- @name
- @version
- @user
- @depends
- @optsdepends
- @extdepends
- @hiercopy

All other fields from [[main]](frontend.html#Section: [main]) section are not allowed.

## [66-enable](66-enable.html) module process creation

The name `foo@` will be used in this explanation as a module name.

When you do `66-enable foo@system`:

- It searches for the corresponding `%%service_system%%/foo@` frontend service file.

- It reads, parses, and replaces `@I` character by `system` as any intantiated service.

- It searches for the corresponding `%%service_module%%/foo@` directory.

- It checks if the `%%service_module%%/foo@/configure`, `%%service_module%%/foo@/service` and `%%service_module%%/foo@/service@` directories exist and create it if it is not the case.

- It verbatim copies the `%%service_module%%/foo@` directory to `%%service_system%%/foo@system`. If the frontend file was found at `%%service_adm%%/foo@` the module, it is copied to `%%service_adm%%/foo@system`, and if the frontend file was found at `$HOME/%%service_user%%/foo@`, the module is copied to `$HOME/%%service_user%%/foo@system`.

	**Note** : if the module already exists and the **-F** was not passed to [66-enable](66-enable.html), the configuration of the modules is skipped. If The **-F** was passed to [66-enable](66-enable.html) the corresponding `%%service_system%%/foo@system` directory is erased and the verbatim copy is made.

- It reads all frontend file found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@` and applies the regex defined with `@infiles` field.

- It reads all frontend file found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@` and applies the regex defined with `@directories` field.

- It reads all frontend file found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@` and applies the regex defined with `@files` field.

- It runs the `%%service_system%%/foo@system/configure` script if it exists.

- It adds the frontend service file set at `@addservices`, if any exist.

- It reads and parses all frontend service files found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@`, as any `66-enable <service>` service process.

- It disables the service `foo@system` if it already exists, and finally finishes the enabling process, like any other service. 
