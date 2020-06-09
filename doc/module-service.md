title: The 66 Suite: module service creation
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[obarun.org](https://web.obarun.org)

# Module service creation

A module can be considered as an [instantiated](instantiated-service.html) service. It works the same way as a service frontend file but allows the user/admin to configure a set of different kind of services before executing the enable process. Also, the set of services can be configured with the conjunction of a script called *configure* which can be made in any language.

This allows one to pre-configure a set of parameters or other services for special tasks without knowing the exact target of the module. The best example is the module for booting a machine. Each machine is different and the set of services need to be adaptable as much as possible for different kinds of machines during boot; e.g booting a container.

A module is defined with two elements:

- an [instantiated frontend service](instantiated-service.html) file at `%%service_system%%` 

- a directory at `%%module_system%%`.

The name of the frontend and the name of the directory **must** be the same. For example if the frontend is named `%%service_system%%/foo@`, the directory of the module must be `%%module_system%%/foo@`.

## Frontend file

The [instantiated frontend service](instantiated-service.html) is written as any other instantiated service with its own specification—see the section [[regex]](frontend.html#Section: [regex]).

## Module directory

The module directory can contain three sub-direcotories:

- *configure* : This directory can contain an **executable** file script named configure. For example, `%%module_system%%/foo@/configure/configure`. The sub-directory **must** be named *configure* and the file script **must** be named *configure*. This file **is not** mandatory. The [66-enable](66-enable.html) process will detect if the file exists and it runs it if it's the case. It's up to you to write the *configure* script file with the language of your choice as long as you define a correct *shebang*.

	Also, this directory can contain any files or directories that you need to configure your module. It's the responsability of the module creator to properly use or dispatch files or directories found inside the *configure* directory with the help of the *configure* script or during the module installation phase. [66-enable](66-enable.html) will not handle any other file than the *configure* script for you. 

	**Note** : The *configure* script is launched after the parsing of the frontend file, meaning all regex operations on directories and files are already made.

- *service* : This directory can contain any frontend files for any kind of service ***except*** instantiated frontend service files. Also, this directory can contain sub-directories containing another frontend service files. This can be done recursively.

- *service@* : This directory can contain any instantiated frontend service files and **only** instantiated frontend service files. Also, this directory can contain sub-directories containing other instantiated frontend service files. This can be done recursively.

Any services that you need **must** be present inside the *service* or *service@* directory. [66-enable](66-enable.html) only deals with these directories. If a service `fooA` has `fooB` as dependency, `fooA` ***and*** `fooB` **must** exist in the `%%module_system%%/<module_name>/service` directory.

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

- It searches for the corresponding `%%module_system%%/foo@` directory.

- It checks if the `%%module_system%%/foo@/configure`, `%%module_system%%/foo@/service` and `%%module_system%%/foo@/service@` directories exist and create it if it is not the case.

- It verbatim copies the `%%module_system%%/foo@` directory to `%%service_system%%/foo@system`. If the frontend file was found at `%%service_adm%%/foo@` the module, it is copied to `%%service_adm%%/foo@system`, and if the frontend file was found at `$HOME/%%service_user%%/foo@`, the module is copied to `$HOME/%%service_user%%/foo@system`.

	**Note** : if the module already exists and the **-F** was not passed to [66-enable](66-enable.html), the configuration of the modules is skipped. If The **-F** was passed to [66-enable](66-enable.html) the corresponding `%%service_system%%/foo@system` directory is erased and the verbatim copy is made.

- It reads all frontend file found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@` and applies the regex defined with `@infiles` field.

- It reads all frontend file found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@` and applies the regex defined with `@directories` field.

- It reads all frontend file found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@` and applies the regex defined with `@files` field.

- It runs the `%%service_system%%/foo@system/configure` script if it exists.

- It adds the frontend service file set at `@addservices`, if any exist.

- It reads and parses all frontend service files found at `%%service_system%%/foo@system/service` and `%%service_system%%/foo@system/service@`, as any `66-enable <service>` service process.

- It disables the service `foo@system` if it already exists, and finally finishes the enabling process, like any other service. 

### Environment variable passed to the script `configure`

At launch of the script `configure`, [66-enable](66-enable.html) passes the following variables to the environment:

- `MOD_NAME`: name of the module.
- `MOD_BASE`: `%%system_dir%%/system` for root, `$HOME/%%user_dir%%/system` for regular user.
- `MOD_LIVE`: `%%livedir%%`.
- `MOD_TREE`: `%%system_dir%%/system/<treename>` for root, `$HOME/%%user_dir%%/system/<treename>` for regular user.
- `MOD_SCANDIR`: `%%livedir%%/scandir/0` for root, `%%livedir%%/scandir/<owner_uid>` for regular user.
- `MOD_TREENAME`: name of the tree.
- `MOD_OWNER`: numerical UID value of the owner of the process.
- `MOD_VERBOSITY`: verbosity level passed to [66-enable](66-enable.html).
- `MOD_COLOR`: color state passed to [66-enable](66-enable.html). `MOD_COLOR=0` means color disabled where `MOD_COLOR=1` means color enabled.
- `MOD_MODULE_DIR`: `%%module_system%%/<module_name>` or `%%module_adm%%/<module_name>` or  `$HOME/%%module_user%%/<module_name>` depending of the owner of the process and the location of the service frontend file.
- `MOD_SKEL_DIR`: `%%skel%%`
- `MOD_SERVICE_SYSDIR`: `%%service_system%%`
- `MOD_SERVICE_ADMDIR`: `%%service_adm%%`
- `MOD_SERVICE_ADMCONFDIR`: `%%service_admconf%%`
- `MOD_MODULE_SYSDIR`: `%%module_system%%`
- `MOD_MODULE_ADMDIR`: `%%module_adm%%`
- `MOD_SCRIPT_SYSDIR`: `%%script_system%%`
- `MOD_USER_DIR`: `%%user_dir%%`
- `MOD_SERVICE_USERDIR`: `%%service_user%%`
- `MOD_SERVICE_USERCONFDIR`: `%%service_userconf%%`
- `MOD_MODULE_USERDIR`: `%%module_user%%`
- `MOD_SCRIPT_USERDIR`: `%%script_user%%`
