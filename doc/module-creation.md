title: The 66 Suite: module service creation
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# Module service creation

A module is an [instantiated](instantiated-service.html) service. It works the same way as a service frontend file but allows the user/admin to configure a set of different kind of services before executing the parse process. Also, the set of services can be configured with the conjunction of a script called *configure* which can be made in any language.

This allows one to pre-configure a set of parameters or other services for special tasks without knowing the exact target of the module. The best example is the module for booting a machine. Each machine is different and the set of services need to be adaptable as much as possible for different kinds of machines during boot; e.g booting a container.

Additionally, a service of type 'module' can be likened to a sandbox. Services within a module are isolated from those outside it, and vice versa. For example, you cannot have a service within a module depending on a service outside the module, and vice versa. The isolation ensures that there is no interdependency between services inside and outside the module.

A module is defined with two elements:

- a directory located in the same directory as any other service [frontend](frontend.html#the-frontend-service-file) file.

- An [instantiated frontend service](instantiated-service.html) file located at the root of that directory.

## Frontend file

The [instantiated frontend service](instantiated-service.html) is written as any other instantiated service with its own specification—see the section [[regex]](frontend.html#Section: [regex]).

## Module directory

The module directory have the following structure:

```
.
├── activated
│   ├── depends
│   └── requiredby
├── configure
│   └── configure
├── frontend
└── module_frontend_file
```

- *activated*: This directory contains empty files named after the services you want to activate for the module. Usually, this directory is populated through the configuration file, but if you manually create empty files named after the service you want to activate, the service will be activated at every configuration of the module regardless of user requests. This directory **is not** mandatory and created at parse process of the module if its doesn't exist yet.

- *activated/depends* and *activated/requiredby*: These directories function similarly to their parent directory, with the exception that they pertain to the dependencies and required-by relationships of the module. Hence, these directories allow you to define dependencies or required-by relationships either through the `configure` script or by manually creating empty files, similar to what can be done in the *activated* directory. These directory **are not** mandatory and created at parse process of the module if they doesn't exist yet.

- *configure*: This directory can contain an **executable** file script named configure. For example, `%%service_system%%/foo/configure/configure`. The sub-directory **must** be named *configure* and the file script **must** be named *configure*. This file **is not** mandatory. The parser will detect if the file exists and it runs it if it's the case. It's up to you to write the *configure* script file with the language of your choice as long as you define a correct *shebang*.

    Also, this directory can contain any files or directories that you need to configure your module. It's the responsability of the module creator to properly use or dispatch files or directories found inside the *configure* directory with the help of the *configure* script or during the module installation phase. The parser will not handle any other file than the *configure* script for you.

    **Note**: The *configure* script is launched after the parsing of the frontend file, meaning all regex operations on directories and files are already made.

- *frontend*: This directory can contain any frontend files for any kind of services. Also, this directory can contain sub-directories containing another frontend service files. This can be done recursively. Frontend file used by the module **must** be present on that directory. You cannot call a frontend file through the `configure` script from outside this directory. Also, do not make a copy of a existing frontend file on your inside this directory. If you want to use an existing frontend file, you **must** rename it differently that the one coming from your system. This directory **is not** mandatory and created at parse process of the module if its doesn't exist yet.

The frontend file of the module itself **must** be present at the root of the module directory.

Any services that you need **must** be present inside the *fronted*. The parser only deals with these directories. If a service `fooA` has `fooB` as dependency, `fooA` ***and*** `fooB` **must** exist in the e.g. `%%service_system%%/<module_name>/frontend` directory.

## A word about the [[main]](frontend.html#Section: [main]) section with the module type

The valid fields in section [[main]](frontend.html#Section: [main]) are:

- @type
- @description
- @name
- @version
- @user
- @depends
- @requiredby
- @optsdepends
- @hiercopy

All other fields from [[main]](frontend.html#Section: [main]) section are not allowed.

## Module process creation

The name `foo@` will be used in this explanation as a module name.

When you do e.g. `66 parse foo@system`:

- It searches for the corresponding `%%service_system%%/foo/foo@` frontend service file.

- It reads, parses, and replaces `@I` character by `system` as any intantiated service.

- It checks if the `%%service_system%%/foo/configure`, `%%service_system%%/foo/frontend` and `%%service_system%%/foo/activated/{depends,requiredby}` directories exist and create it if it is not the case.

- It verbatim copies the `%%service_system%%/foo` directory to `%%service_system%%/foo@system`.

- It applies the regex defined with `@infiles` field to `%%service_system%%/foo@system/frontend` files.

- It applies the regex defined with `@directories` field to `%%service_system%%/foo@system/frontend` directories, if any.

- It applies the regex defined with `@files` field to `%%service_system%%/foo@system/frontend` files.

- It runs the `%%service_system%%/foo@system/configure` script if it exists.

- It reads and parses all frontend services files found at `%%service_system%%/foo@system/activated` and its `depends` and `requiredby` subdirectories.

### Environment variable passed to the script `configure`

At launch of the script `configure`, the parser passes the following variables to the environment:

- `MOD_NAME`: name of the module.
- `MOD_BASE`: `%%system_dir%%/system` for root, `$HOME/%%user_dir%%/system` for regular user.
- `MOD_LIVE`: `%%livedir%%`.
- `MOD_TREE`: `%%system_dir%%/system/<treename>` for root, `$HOME/%%user_dir%%/system/<treename>` for regular user.
- `MOD_SCANDIR`: `%%livedir%%/scandir/0` for root, `%%livedir%%/scandir/<owner_uid>` for regular user.
- `MOD_TREENAME`: name of the tree.
- `MOD_OWNER`: numerical UID value of the owner of the process.
- `MOD_VERBOSITY`: verbosity level passed to parser.
- `MOD_COLOR`: color state passed to the parser. `MOD_COLOR=0` means color disabled where `MOD_COLOR=1` means color enabled.
- `MOD_MODULE_DIR`: path of the module directory.
- `MOD_SKEL_DIR`: `%%skel%%`
- `MOD_SERVICE_SYSDIR`: `%%service_system%%`
- `MOD_SERVICE_ADMDIR`: `%%service_adm%%`
- `MOD_SERVICE_ADMCONFDIR`: `%%service_admconf%%`
- `MOD_SCRIPT_SYSDIR`: `%%script_system%%`
- `MOD_USER_DIR`: `%%user_dir%%`
- `MOD_SERVICE_USERDIR`: `%%service_user%%`
- `MOD_SERVICE_USERCONFDIR`: `%%service_userconf%%`
- `MOD_SCRIPT_USERDIR`: `%%script_user%%`
