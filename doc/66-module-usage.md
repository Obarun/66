# Using a module

A **module** is a service that expands into a whole set of services, brought up as an
[instance](66-instantiated-service.html). For **what** a module is for and when to reach for
one, see [module services](66-module.html); to build your own, see
[creating a module](66-module-creation.html). This page is the user's side — how to run,
configure and manage one. The running example is `webapp@`, the module built there: a web
application instance made of a `server` and an optional `worker`.

## Starting an instance

A module is instantiated like any other instantiated service — the name after the `@` is the
instance:

```
66 start webapp@blog
```

`webapp@` is the module, `blog` the instance. If it was never parsed before, `66` parses it
first, with the module author's default configuration.

You usually [enable](66-enable.html) it so it comes back on the next boot:

```
66 enable webapp@blog
```

or enable and start in one pass:

```
66 enable -S webapp@blog
```

## Configuring an instance

The point of a module is that each instance is **configurable**. Its tunables are the
variables of the module's `[Environment]` — for `webapp@`, whether the worker runs. Edit them
with [66 configure](66-configure.html):

```
66 configure webapp@blog
```

Changes do **not** take effect until you re-parse the instance with
[66 reconfigure](66-reconfigure.html):

```
66 reconfigure webapp@blog
```

Run `reconfigure` every time you change an instance's configuration; until you do, the
instance keeps its previous one. Setting `WEBAPP_WORKER=no` and reconfiguring, for example,
drops the `worker` from `webapp@blog` on the next parse.

## Managing the services inside a module

Starting a module starts several services at once. List them with
[66 status](66-status.html) — the `contents` field holds the services the instance expanded
to:

```
66 status webapp@blog
```

Address any one of them with the `module:service` name — the module instance, a colon, and
the inside service:

```
66 stop webapp@blog:worker
```

The colon separates the instance (`webapp@blog`) from the inside service (`worker`); it is the
service's real, stored name, so every `66` command accepts it:

```
66 restart webapp@blog:server
```

If an inside service is itself instantiated, give its full name after the colon:

```
66 reload webapp@blog:cache@ro
```
