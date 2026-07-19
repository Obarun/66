# Creating a module

A **module** is a service of [`Type = module`](66-frontend.html#type) that expands, at
[parse](66-parse.html) time, into a whole **set** of services carried in its own directory.
It is an [instantiated service](66-instantiated-service.html): you write it once as
`webapp@` and the admin brings up as many independent instances as needed — `webapp@blog`,
`webapp@shop`, … — each parsed from the same template with its instance name substituted in.

For **what** a module is for — the problems it solves and the properties that make it a real
unit of composition (isolation, one-artifact packaging, per-instance configuration) — see
[module services](66-module.html). This page is the author's side: it builds one module from
scratch — `webapp@`, a web application instance made of a **server** and an optional
**worker** — and explains each moving part as we add it. To use a module once it exists, see
[module usage](66-module-usage.html).

## The module we will build

`webapp@blog` should bring up:

- `server` — the web server for the `blog` instance, always present;
- `worker` — a background worker, present only when the instance asks for it.

The instance name (`blog`) must flow into both services, the listening address is fixed by
us (the module author), and whether the worker runs is a per-instance choice the admin makes
with [66 configure](66-configure.html). Every one of those needs is met by a distinct part of
the module, introduced below.

## Anatomy of a module directory

A module lives in a directory named like the module — `webapp@` — placed **among your other
service frontends** (e.g. `%%service_system%%/webapp@`). Ours will end up like this:

```
webapp@/
├── webapp@              # the module frontend (Type = module)
├── frontend/
│   ├── server          # a normal frontend, the instance's web server
│   └── worker          # a normal frontend, the instance's worker
├── configure/
│   └── configure       # optional script: per-instance decisions
└── activated/
    ├── server          # empty file → 'server' is part of every instance
    ├── depends/        # (module's dependencies on OUTSIDE services)
    └── requiredby/     # (services OUTSIDE that require this module)
```

Only the module frontend and the `frontend/` services are really yours to write; `66`
creates the `configure/`, `activated/`, `activated/depends` and `activated/requiredby`
directories at parse time if you leave them out. We fill them in on purpose.

## Step 1 — the module frontend

The file at the root of the directory, named exactly like the module (`webapp@`), declares
the service as a module. It is a normal [frontend](66-frontend.html), restricted to the keys
a module understands (see [Allowed keys](#allowed-keys)), plus the module-only
[`[Regex]`](66-frontend.html#section-regex) section that drives the expansion.

```ini
# webapp@
[Main]
Type = module
Version = 0.1.0
Description = "web application instance @I"
User = ( root )

[Environment]
WEBAPP_WORKER=yes

[Regex]
Configure = "@I"
InFiles = ( :server:LISTENADDR=0.0.0.0:8080 ::INSTANCE=@I )
```

`@I` is an [identifier](66-identifier.html): for `webapp@blog` it expands to the instance
name, **`blog`** (the part after the `@`). In the **module frontend itself** (`webapp@`,
parsed as `webapp@blog`), identifiers are substituted directly, so `Description` above becomes
`"web application instance blog"`, and the `InFiles` value `@I` becomes `blog`. `Configure`
takes a [quoted](66-frontend.html#quote) value, so `@I` must be written `"@I"`.

Inside services are different: an identifier there is **not** the instance (see
[Step 2](#step-2-the-inside-services)). That is why the instance is carried into them through
the `::INSTANCE=@I` `InFiles` rule above.

`[Environment]` holds the module's tunables. `WEBAPP_WORKER` is the knob the admin flips per
instance with [66 configure](66-configure.html); we read it from the
[configure script](#step-4-the-configure-script) below.

The `[Regex]` keys are explained in [Step 3](#step-3-the-regex-transformations).

## Step 2 — the inside services

Everything the module runs lives in `frontend/`, as ordinary frontends. Write the local name
you want to address later (`server`, `worker`); dependencies between inside services use
those same local names.

```ini
# frontend/server
[Main]
Type = classic
Description = "web server for INSTANCE"

[Start]
Execute = ( httpd -listen LISTENADDR -name INSTANCE )
```

```ini
# frontend/worker
[Main]
Type = classic
Description = "background worker for INSTANCE"
Depends = ( server )

[Start]
Execute = ( webapp-worker --app INSTANCE )
```

`worker` depends on `server`: a dependency **inside** the module. It resolves to the module's
own `server` because `66` looks the name up in `frontend/` only — this is the isolation rule.
A `Depends = ( something-outside )` here would fail at parse: an inside service cannot reach
out.

**Careful with `@I` inside a member:** it does **not** expand to the instance here. Each inside
service is parsed as its own frontend named after its local name, so `@I` in `frontend/server`
would become `server`, not `blog`. To use the instance name inside a member, carry it in with an
[`InFiles`](#step-3-the-regex-transformations) rule — that is what `INSTANCE` is above: the
module's `::INSTANCE=@I` rule replaces it with `blog`. `LISTENADDR` is filled the same way, next.

## Step 3 — the `[Regex]` transformations

When `66` parses `webapp@blog` it copies the whole `webapp@` directory to a working
`webapp@blog` and rewrites the copy through the [`[Regex]`](66-frontend.html#section-regex)
keys, in this fixed order: **`InFiles` → `Directories` → `Files` → `configure`**. Identifiers
such as `@I` are already resolved when the module frontend is read — in its own keys and in
these regex *values* — they are **not** re-applied to the inside frontends, which resolve their
own identifiers against their local names. The four regex keys:

- **`InFiles`** — replace text *inside* the `frontend/` files. `:name:regex=value` targets one
  file; `::regex=value` targets all. Our module uses it to set the listening address on
  `server` and to carry the instance name into every inside service:

  ```ini
  InFiles = ( :server:LISTENADDR=0.0.0.0:8080 ::INSTANCE=@I )
  ```

  turns `httpd -listen LISTENADDR -name INSTANCE` into `httpd -listen 0.0.0.0:8080 -name blog`.
  The replacement value may itself contain identifiers — `@I` is expanded (against the *module*
  frontend, so it is the instance) before the regex runs — so `::SOCK=/run/@I.sock` would inject
  `/run/blog.sock`.

- **`Directories`** — rename sub-directories of `frontend/`. `Directories = ( DM=sddm )`
  renames `use-DM/` to `use-sddm/`.

- **`Files`** — rename files, same rule as `Directories`. `Files = ( GENERIC=@I )` renames the
  file `GENERIC` to `blog`.

- **`Configure`** — the value passed as `$1` to the [configure script](#step-4-the-configure-script).
  It takes a [quoted](66-frontend.html#quote) value; we pass `"@I"`, so the script receives `blog`.

`webapp@` needs only `InFiles` and `Configure`; `Directories`/`Files` are shown for
completeness. Keys you do not use may be omitted.

## Step 4 — the configure script

`configure/configure` is an **optional executable** run once per parse, **after** the regex
passes and **before** the inside services are read. That timing is the point: the script can
edit the working copy — most usefully, populate `activated/` — so the set of services is
decided *dynamically*, per instance.

```bash
#!/usr/bin/bash
# $1 is the [Regex] Configure value — here, the instance name.
# WEBAPP_WORKER comes from [Environment]; `66 configure` lets the admin override it.

instance="$1"

if [ "${WEBAPP_WORKER}" = yes ] ; then
    touch ../activated/worker      # cwd is the module's configure/ directory
else
    rm -f ../activated/worker
fi

echo "configured webapp instance ${instance}, worker=${WEBAPP_WORKER}" >&2
```

The script runs with its working directory set to the module's `configure/` directory, so
`../activated/` is the sibling to write into. If the script exits non-zero the **whole parse
fails** — validate inputs and let a real error stop the build rather than shipping a broken
instance. Its environment carries the module's `[Environment]` merged with a set of
`MOD_*` variables (see [Configure environment](#configure-environment)).

## Step 5 — activation

`activated/` decides which inside services actually start. Each **empty file** there names one
service from `frontend/` to bring up. A file committed in the module sources is activated for
**every** instance; a file created by the configure script is activated **conditionally**.

Our module ships `activated/server` (the server is always part of an instance) and lets the
configure script add `activated/worker` when `WEBAPP_WORKER=yes`. So `webapp@blog` with the
default environment starts `server` **and** `worker`; an instance configured with
`WEBAPP_WORKER=no` starts `server` alone.

`activated/depends/` and `activated/requiredby/` are covered in
[Depending on outside services](#depending-on-outside-services).

## Step 6 — parse and verify

Install the `webapp@` directory beside your other frontends, then parse an instance:

```
66 parse webapp@blog
```

`66` performs the copy, the regex passes, the configure script and finally reads the
activated services. Inspect the result with [66 status](66-status.html):

```
66 status webapp@blog
```

The `contents` field lists the services the module expanded to — for us,
`webapp@blog:server` and (with the default environment) `webapp@blog:worker`. From there,
[enable](66-enable.html) and [start](66-start.html) the instance, and address the inside
services by their full `module:service` name — see [module usage](66-module-usage.html).

## Depending on outside services

Isolation forbids an *inside* service from depending on an *outside* one, but the **module
itself** may depend on outside services. Declare those on the module frontend with
[`Depends`](66-frontend.html#depends) / [`RequiredBy`](66-frontend.html#requiredby), or drop
empty files into `activated/depends/` and `activated/requiredby/` (which the configure script
can populate dynamically, exactly like `activated/`). If `webapp@` needs a shared database to
be up first:

```ini
# in webapp@'s [Main]
Depends = ( postgresql )
```

`postgresql` is resolved as a normal, outside service. The isolation rule only bites on the
services *inside* `frontend/`.

## Reference

### Allowed keys

A module frontend is parsed for a subset of the frontend keys; the rest are **silently
ignored** (no error), because a module runs no process of its own.

- **[`[Main]`](66-frontend.html#section-main)** — honoured: `Type`, `Description`, `Version`,
  `User`, `Depends`, `RequiredBy`, `OptsDepends`, `Provide`, `Conflict`, `Flags`, `InTree`,
  `CopyFrom`. The execution keys (`Execute`, `RunAs`, …) and the `[Start]`/`[Stop]`/`[Logger]`
  sections are ignored — a module has no run script.
- **[`[Environment]`](66-frontend.html#section-environment)** — the module's tunables, exposed
  to the configure script and overridable with [66 configure](66-configure.html).
- **[`[Regex]`](66-frontend.html#section-regex)** — `Configure`, `InFiles`, `Directories`,
  `Files`, as in [Step 3](#step-3-the-regex-transformations).

`contents` is **not** an authoring key: `66` computes it from the services the module
expanded to. A `Contents = …` written by hand is ignored and overwritten.

### Parse order

For `66 parse webapp@blog`, `66`:

1. substitutes [identifiers](66-identifier.html) (`@I` → `blog`) in the module frontend;
2. copies `%%service_system%%/webapp@` to the working `webapp@blog`;
3. applies `InFiles`, then `Directories`, then `Files` to the `frontend/` copy;
4. runs `configure/configure` (if present);
5. reads and parses the services listed in `activated/`, plus `activated/depends` and
   `activated/requiredby`.

### Configure environment

Besides the module's own `[Environment]`, the configure script receives these variables. Its
first argument (`$1`) is the `[Regex] Configure` value.

| Variable | Value |
|---|---|
| `MOD_NAME` | the module name |
| `MOD_BASE` | the owner's system directory (`%%system_dir%%/system` for root, `$HOME/%%user_dir%%/system` for a user) |
| `MOD_LIVE` | `%%livedir%%` |
| `MOD_SCANDIR` | the scandir path |
| `MOD_TREENAME` | the tree the instance is parsed into |
| `MOD_OWNER` | numerical UID of the process owner |
| `MOD_COLOR` | `1` if colour is enabled, else `0` |
| `MOD_VERBOSITY` | verbosity level passed to the parser |
| `MOD_MODULE_DIR` | path of the module's working directory |
| `MOD_SKEL_DIR` | `%%skel%%` |
| `MOD_SERVICE_SYSDIR` | `%%service_system%%` |
| `MOD_SERVICE_ADMDIR` | `%%service_adm%%` |
| `MOD_SERVICE_ADMCONFDIR` | `%%service_admconf%%` |
| `MOD_SCRIPT_SYSDIR` | `%%script_system%%` |
| `MOD_ENVIRONMENT_ADMDIR` | `%%environment_adm%%` |
| `MOD_USER_DIR` | `%%user_dir%%` |
| `MOD_SERVICE_USERDIR` | `%%service_user%%` |
| `MOD_SERVICE_USERCONFDIR` | `%%service_userconf%%` |
| `MOD_SCRIPT_USERDIR` | `%%script_user%%` |
| `MOD_ENVIRONMENT_USERDIR` | `%%environment_user%%` |
