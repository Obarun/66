# Module services

A **module** is a service of [`Type = module`](66-frontend.html#type) that, at
[parse](66-parse.html) time, expands into a whole **set** of services managed as one. You author
it once as an [instantiated service](66-instantiated-service.html) — `webapp@` — and stamp out as
many independent instances as you need — `webapp@blog`, `webapp@shop`, … — each a complete,
self-contained bundle parsed from the same template.

## The shape of a module

Picture one template you stamp into sealed bundles of services:

```
       webapp@                       the template — you write it once
          │
          │  instantiate:  webapp@blog, webapp@shop, …
          ▼
   ┌── webapp@blog ─────────┐
   │                        │        each instance is a sealed
   │   worker ──▶ server    │        bundle: its services depend
   │     └─────▶ cache      │        on one another, privately
   │                        │
   └───────────┬────────────┘
               │                     only the bundle — not its
               ▼                     insides — reaches outward
          postgresql                 (a module-level dependency)
```

Three ideas are the whole concept:

- **A template, stamped out.** You write `webapp@` once; every `webapp@<name>` is an independent
  copy with its instance name substituted in. The template is what you *distribute*; an instance
  is what you *run*.
- **A sealed bundle.** Inside an instance, services depend on one another freely, and that
  dependency graph is *private*: nothing inside may reach out, and nothing outside may reach in.
  The wall is real, not a convention.
- **One handle, one interface.** You address the whole bundle by its instance name, and the
  bundle — not its insides — is the single thing that connects to the rest of the system.

Everything below follows from those three.

## What a module gives you

### One artifact, many instances

The template, its inside services, its tunables and its expansion rules all live in **one
directory** (`webapp@/`). That directory *is* the deliverable — ship it, and an admin stamps out
`webapp@blog`, `webapp@shop`, … at will. Each is parsed independently, so instances never share
state, never collide, and are started, enabled or removed on their own.

### The whole stack in one command

Starting an instance brings up **all** of its inside services at once, in dependency order — the
module already knows what it contains, so you never wire them up or start them one by one:

```
66 start webapp@blog        # server, worker and cache come up together
```

### Configured as a whole, per instance

A module carries its tunables in [`[Environment]`](66-frontend.html#section-environment), and each
instance can be set differently with [66 configure](66-configure.html). The *shape of the set*
can vary too: a module's [configure script](66-module-creation.html#step-4-the-configure-script)
runs at parse time and decides which inside services take part — one instance with a worker,
another without, from the same sources and with no duplication.

### Safe to evolve

Because the wall is real, a module's internals are yours to change — add a service, rename one,
split one in two — with no risk to the rest of the system, since nothing out there was ever
allowed to reach in. The system's dependency graph stays a set of clean, self-contained bundles
instead of one sprawling web where anything can hook onto anything.

## Problems a module solves

Reach for a module whenever you ship **several services that belong together** and must be
**instantiated** and **configured** as a unit.

- **A per-site application stack.** `webapp@blog`, `webapp@shop`: each site is a server, a worker
  and whatever else the app needs, brought up and torn down as one, configured per site.
- **A per-interface network bundle.** `net@eth0`, `net@wlan0`: address configuration, a DHCP
  client, a supervisor — one bundle per interface, isolated from the others.
- **The boot sequence of a machine.** A machine's early services form a set that is ordered,
  isolated, and enabled or disabled as a block — exactly a module.
- **Any reusable group with a variable shape.** When *how many* or *which* services depend on
  per-instance configuration, the [configure script](66-module-creation.html#step-4-the-configure-script)
  decides the set at parse time; you never maintain N near-identical copies by hand.

If you only need to *order* or *co-start* a few fixed services, a plain
[dependency](66-dependencies.html) between them is simpler — a module earns its keep when
instantiation, per-instance configuration and isolation come together.

## Next steps

- [Creating a module](66-module-creation.html) — build one from scratch, part by part.
- [Using a module](66-module-usage.html) — start, configure and manage a running instance.
- [Instantiated services](66-instantiated-service.html) — the `@` template mechanism a module
  builds on.
