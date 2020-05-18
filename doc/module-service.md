title: The 66 Suite: module service creation
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# Module service creation

A module can be considered as an [instantiated](instantiated-service.html) service. It works as the same way as a service frontend file but allows to configure a set of different kind of services before executing the enable process. Also, the set of the services can be configured with the conjunction of a script called *configure* which it can be made on any language.

This allows to pre-configure a set of services for a special tasks without knowing the exact target of the module. The best example is the module for the boot of a machine. Each machine are different and the set of the services need to be adaptable as much as possible for different kind of machine or boot, e.g booting a container.

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

	**Note** : The *configure* script is launched after the parse of the frontend file meaning all regex operation on directories and files are already made.

- *service* : This directory can contain any frontend files for any kind of service ***except*** instantiated frontend service file. Also, this directory can contain sub-directories containing another frontend service files. This can be done recursively.

- *service@* : This directory can contain any instantiated frontend service file and **only** instantiated frontend service file. Also, this directory can contain sub-directories containing another instantiated frontend service files. This can be done recursively.

Any services that you need **must** be present inside the *service* or *service@* directory. [66-enable](66-enable.html) only deal with these directories. If a service `fooA` have `fooB` as dependency, `fooA` ***and*** `fooB` **must** exist at  `%%service_module%%/<module_name>/service` directory.

## A word about the [[regex]](frontend.html#Section: [regex]) section `@addservices` field

You may need a existing frontend service file from your system to configure your module. Instead of redefine this frontend file, use this field. The [66-enable](66-enable.html) process will verbatim copy the frontend service file inside `%%service_system%%/<module_name>/service` or `%%service_system%%/<module_name>/service@` module directory depending of the type of the service. This copy is made **after** all regex operations and do not modifies the frontend file at all.

## A word about the [[main]](frontend.html#Section: [main]) section with the module type

The valid field in section [[main]](frontend.html#Section: [main]) are:

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

The name `foo@` will be used in this explanation as module name.

When you do `66-enable foo@system`:

- It searches for the corresponding `%%service_system%%/foo@` frontend service file.

- It reads, parses it and replace `@I` character by `system` as any intantiated service.

- It searches for the corresponding `%%service_module%%/foo@` directory.

- It checks if the `%%service_module%%/foo@/configure`, `%%service_module%%/foo@/service` and `%%service_module%%/foo@/service@` directories exist and create it if it is not the case.

- It verbatim copy the `%%service_module%%/foo@` directory to `%%service_system%%/foo@system`. If the frontend file was found at `%%service_adm%%/foo@` the module is copied to `%%service_adm%%/foo@system` and if the frontend file was found at `$HOME/%%service_user%%/foo@` the module is copied to `$HOME/%%service_user%%/foo@system`.

	**Note** : if the module already exist and the **-F** was not past to [66-enable](66-enable.html), the configuration of the modules is skipped. If The **-F** was past to [66-enable](66-enable.html) the corresponding `%%service_system%%/foo@system` directory is erased and the verbatim copy is made.

- It reads all frontend file found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@` and apply the regex define with `@infiles` field.

- It reads all frontend file found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@` and apply the regex define with `@directories` field.

- It reads all frontend file found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@` and apply the regex define with `@files` field.

- It run the `%%service_system%%/foo@system/configure` script if exist.

- It add the frontend service file set at `@addservices` if any.

- It reads and parses all frontend service files found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@` as any `66-enable <service>` service process.

- It disable the service `foo@system` if it already exist and finally finish the enable process like any other services. 
