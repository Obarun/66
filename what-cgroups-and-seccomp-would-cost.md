# What cgroups and seccomp would cost 66

Two features 66 does not have, costed out honestly: per-service resource control, which is
on the roadmap, and seccomp filtering, which belongs somewhere else in the suite. These are
the gaps most often raised against 66, and both turn out cheaper than they look, for the
same reason in each case. 66 already solves by other means the expensive half of what
systemd uses these mechanisms for.

This is an internal design note. It ships with the source rather than with the binary, it
is not part of the published documentation, and it exists to back two roadmap entries with
an actual estimate instead of a guess. The user-facing counterpart is
[*Where 66 stands among init systems*](https://docs.obarun.org/66/latest/66-vs-other-init-systems.html),
which places 66 among the alternatives; this one prices the remaining work.

## Two jobs routinely confused

"66 has no cgroups" is only interesting once you separate the two jobs cgroups are made
to do:

1. **Tracking and teardown.** Knowing which processes belong to a service, and killing
   all of them on stop.
2. **Resource control and accounting.** Capping memory, weighting CPU, limiting tasks,
   and reading what was consumed.

cgroups were built for the second. systemd uses them for the first because it has no
alternative in its model: a unit's processes are ordinary host processes, so a daemon
that double-forks reparents to the host's PID 1 and is lost, unless every process carries
a label that cannot be shed. The cgroup is that label, and `cgroup.kill` is the sweep over
it.

66 does not need that. A service started under `66-ns -o unshare=pid` gets `66-ns` as
PID 1 of its own PID namespace. A daemon that backgrounds itself reparents to *that*
PID 1 — which can then `wait()` for it like any direct child — instead of escaping to the
host. And when that PID 1 exits, the kernel sends `SIGKILL` to every remaining process in
the namespace and destroys it. Teardown is a kernel guarantee, atomic and total; there is
no sweep for the manager to perform correctly, no leaked cgroup to `rmdir`, and no pid
file needed just to keep track of a backgrounded daemon. `cgroup.kill` is the workaround
for not having any of that.

One caveat: a PID namespace is requested per service, through `Execute`, where systemd's
cgroup tracking is automatic for every unit. The guarantee applies to services that ask
for it. That is a packaging convention to establish, not a missing mechanism.

What genuinely remains missing is job 2, and only job 2: `memory.max`, `cpu.weight`,
`pids.max`, io throttling, and the counters that go with them. rlimits do not cover it —
they are per process, and `RLIMIT_NPROC` counts per user rather than per service.

Job 2 is on the 66 roadmap. The distinction above is not a justification for leaving the
gap open; it is what makes closing it cheap. Because tracking and teardown are already
answered by the PID namespace, 66 has no reason to adopt the cgroup as a process label,
and therefore inherits none of the machinery that follows from that choice: no
unit-to-cgroup lifetime that teardown depends on, no `cgroup.kill` sweep to get right, no
escaped process to hunt down. What is planned is the resource layer alone. Write a handful
of values into a per-service cgroup directory before `exec`, read the counters back out.
The estimate below is for that layer, and it is small precisely because it does not carry
job 1 with it.

## Where a resource knob belongs

Which refines the criterion for splitting the core from the tools. The line is not
before-`exec` versus after-`exec`; rlimits, `nice`, umask and capabilities are all applied
before `exec` and all live in 66 already. It is this:

> **An *attribute* of the service (what it is allowed to consume, who it runs as, how it
> is scheduled) belongs in the frontend and is applied by `66-execute`. A *confinement*
> of the service (what it can see, reach, or call) belongs in `66-ns`.**

`MemoryMax` is an attribute, in the same family as `LimitAS`, and it should sit next to
it in `[Execute]`. Namespaces and seccomp are confinement. The split stays clean, and
nothing has to move.

## cgroups — cost

### Where the code goes

The exec chain in `66-execute.c` (lines 841–861) is already the single place where every
process attribute is applied, in order: io, environment, script, rlimits, nice,
privileges, capabilities, uid/gid, umask, chdir, then `exec`. Creating the leaf, writing
the knobs and joining it is one more step in that list. cgroup membership is inherited
across `fork`, so writing the PID once before `exec` covers the whole descendant tree.

Because teardown is not part of the deal, 66-supervise is not touched at all. No lifecycle
in the supervisor, no `cgroup.kill`, no `rmdir` race, and no change to what `stop` means.
That was the risky, expensive half of the feature, and PID namespaces delete it.

### What still costs

**1. Hierarchy layout and the kernel's rules.** cgroup v2 forbids a cgroup from holding
processes while it has controller-enabled children, and controllers must be enabled
downward through `cgroup.subtree_control` at every level. A layout such as
`/sys/fs/cgroup/66.slice/<tree>.slice/<service>/` satisfies it, since services are always
leaves, but it becomes part of 66's public contract, and since trees are already a
first-class concept the mapping has to be deliberate.

**2. The addon.** The `limit` addon is the pilot for exactly this shape and its footprint
is known: roughly 16 files (a `parse_*.c`, the CDB write/read pair, sanitize,
`modify_field`, `get_field`, the four generic `resolve_*` dispatchers, the hash,
`ssexec_resolve`, three headers, the enum table) plus doc and tests. A `.cgroup` addon
carrying `MemoryMax`, `CPUWeight`, `TasksMax` follows a well-trodden path.

**3. User-scope delegation.** Root scope is straightforward: 66 owns `66.slice` and
writes to it. User scope needs a subtree chowned to the user with delegation containment
respected; on a systemd host that is what `user@.service` provides. Here it would be a
root-side step at boot, and `66-userd` is the natural place for it, since it already owns
the user's runtime directory and runs at exactly the right moment. Still a design
decision, but no longer a homeless one.

**4. Graceful degradation.** A service with no cgroup key simply gets no cgroup. The
feature is additive rather than load-bearing, which is the difference between a two-day
risk and a two-week one.

**5. The payoff beyond limits.** Once the leaf path is known, `memory.current`,
`cpu.stat` and `pids.current` are three file reads away: per-service resource reporting
in `66 status` for almost nothing.

### Estimate

| Phase | Content | Cost |
|---|---|---|
| 1 | `.cgroup` addon + frontend keys + doc + tests | 2–3 days, mechanical |
| 2 | Hierarchy at boot, create/write/join in `66-execute` | 3–4 days |
| 3 | Counters surfaced in `66 status` | 2 days |
| 4 | User-scope delegation via `66-userd` | design first, then ~3 days |

Roughly one to one and a half weeks for a credible root-scope v1, rather than the two to
three it would otherwise take. The difference is entirely the supervisor work that is not
needed: refusing to use cgroups as a tracking substrate is what makes cgroups cheap to add
as resource control.

## seccomp

The kernel side is trivial. `prctl(PR_SET_NO_NEW_PRIVS, ...)` is already done in
`66-execute`, and installing a filter is one `seccomp(SECCOMP_SET_MODE_FILTER, ...)` call
next to it. Perhaps 30 lines.

The cost is entirely in *producing the filter*. A seccomp filter is a raw BPF program
over syscall numbers, and syscall numbers are per-architecture: x86_64, x32 and i386
multiplex in the same process, ARM has its own table, and the numbers move with kernel
releases. Writing this by hand means reimplementing libseccomp's architecture tables and
rule resolver, thousands of lines, with a maintenance obligation on every kernel release.
By the project's own dependency test (*would rewriting it myself take real time for a
worse result?*) libseccomp is a justified dependency, if the feature is done at all.

But two things argue against putting it in 66:

- **It reverses the direction the project just took.** 0.9.0.0 removed skalibs, s6 and
  execline, leaving oblibs as the single dependency and one the project controls. Taking on
  an external LGPL C library right after that is a strategic reversal, and it interacts
  badly with the existing `enable-static-deps` / `enable-static-executable` build options.
  See the licensing note below.
- **It is confinement, not an attribute.** A syscall filter restricts what a process may
  *call*, which is the same family as what it may see or reach. By the line drawn above it
  belongs with the namespaces, and composes through `Execute` the same way they do.

### The licensing question, stated precisely

libseccomp is LGPL-2.1. That does **not** relicense anything: 66 and 66-tools are both
ISC and both stay ISC. The LGPL's reciprocity covers the library and modifications *to
it*, not the program that calls it. What it attaches instead are distribution conditions
on the resulting binary, and they differ sharply by linking mode:

- **Dynamically linked**, which is effectively free. Ship a notice that libseccomp is used
  and covered by the LGPL, ship the licence text, and the user can already replace the
  `.so`. Nothing else changes, and this is how every distribution package would build it
  anyway.
- **Statically linked.** The licence still does not spread, but the user's right to relink
  against a modified libseccomp has to be preserved concretely: distribute the object
  files, or a relinkable form of the program, alongside the binary.

So the real cost is a build-mode constraint rather than legal exposure. A fully static
66-tools with seccomp compiled in stops being a single self-contained artefact you can
drop anywhere without shipping something extra beside it. For a suite that advertises
`enable-static-executable`, that is a genuine loss even though the licence of the code
never moves.

Which leaves a third route the trade-off above skips. libseccomp earns its keep when the
goal is a *general* filter language: arbitrary syscall names, argument matching, and the
multi-architecture tables that make x86_64/x32/i386 multiplexing safe. If `66-ns` instead
ships a fixed set of curated profiles for the build architecture, the problem shrinks to
what `<asm/unistd.h>` already provides at compile time. Emit a linear cBPF chain over
`__NR_*` constants, and have the filter reject any `seccomp_data.arch` that is not the one
it was built for, which a correct filter must do regardless. That is bounded, ISC-clean
and statically linkable, and the only thing it gives up is generality. The choice between
the two should be made on how much generality the profiles actually need, not on the
licence.

So seccomp belongs in `66-ns`, alongside the namespaces it already owns. Cost to 66: zero,
plus one documentation section showing the composition. Cost to 66-tools: a profile parser,
plus either libseccomp as a dependency or a hand-rolled native-arch emitter. A contained,
well-understood piece of work in the program whose job this already is.

That also keeps the story straight: 66 manages services, `66-ns` confines them,
`66-userd` follows the people using them. A service manager that starts absorbing
sandboxing directives is a service manager on its way to becoming a platform, and the
whole point of the placement in the companion document is that 66 declined that road on
purpose.
