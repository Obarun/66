# Command contracts

This page states how each `66` command reacts in each situation: its exit code, what it does to the services, and the message it prints. A command behaves exactly as written here.

Each table uses the chain `a <- b <- c` when an example is needed: `c` depends on `b`, `b` depends on `a`.

## General rules

Nothing under the directories of 66 is edited by hand. A case born of a hand on its files, a resolve file, a tree, a link of the Provide directory or of the scandir removed or overwritten, is outside this contract and left as it is, a damaged resolve file aside, which 66 names and refuses as written under [Invalid service](#invalid-service); the repairs 66 offers are the commands it names (`66 reconfigure -c`, `66 parse -f`, `66 remove`, and for a tree `66 tree remove` then `66 tree create`).

### Exit codes

| code | meaning |
|---|---|
| `0` | success |
| `100` | the caller has to act: a missing or wrong argument, an unknown name, a state the caller has to establish first (scandir not running, service not parsed, not supervised, not running, conflicting service), an invalid frontend, an invalid service met by the command |
| `111` | the system failed: a system call, a service that fails to start or to stop |

### Refusal

A command that refuses changes nothing: nothing is parsed, written, enabled, started or stopped. Three exceptions:

- `66 enable -S`: enabling and starting are two distinct actions, and the activation stays when the start fails or is refused.
- `66 disable -S`: disabling and freeing are two distinct actions, and the deactivation stays when the freeing fails or is refused.
- A refusal that needs the graph of the system, an invalid service, a conflicting service, or a provided name an enabled provider already holds, met by the command, comes after the parse on the fly: a service parsed by the command stays parsed, exactly as `66 parse` would have left it. Nothing is enabled, started or stopped. A name refused before the graph, an unknown name or a module member named alone, leaves nothing parsed.

A command naming several services refuses all of them when one of them is refused.

`66 reconfigure` checks the frontends again under the lock, before it writes: another command may have written meanwhile. Refused there, it writes nothing and puts the services it took out of the supervision back in the state it found them in, then exits with `100`.

### Failure

A service that fails while a command acts, a start or a stop that fails, a script that exits non zero, a service its supervisor kills at its timeout, stops only what waits for it: what depends on it for a start, what it depends on for a stop. The other services of the selection go to the end of the action and of what follows it: unsupervised, disabled, reloaded, armed again. The command exits with `111`.

- A service killed by its supervisor at its `[Stop] Timeout` is down: it counts as stopped, what waits for it goes on.
- `66 remove` and `66 reconfigure` write nothing when a service fails: what they took out of the supervision is put back in the state they found it in.
- A command whose time runs out (**-T**) or that receives a signal stops where it is: nothing more is sent, what is down stays supervised, and it exits with `111`.

### Names

A name that has no frontend and that no service answers to is refused with `100` by every command, a command that parses (`parse`, `start`, `enable`, `reconfigure`, `status`) before parsing anything:

```
service: <name> is unknown: no frontend and no service answers to the name -- is its provider enabled?
```

A name with a frontend and no resolve file is refused with `100` by every command that does not parse it:

```
service: <name> is not parsed -- parse it with 66 parse <name>
```

A name provided by a service through its `Provide` key answers for that service, except for `66 remove`, which takes a name literally. A parsed service keeps its name while it is enabled; disabled, an enabled provider may take the name over, the service staying parsed, and `66 enable` of the service takes the name back from a disabled provider.

A damaged entry of the Provide directory, a file that is no link or a link to nothing, provides nothing: it is ignored, the name designates the service bearing it, and the entry is named at verbosity 2:

```
damaged Provide entry, ignored: <path>
```

The suffix `-log` is reserved to the loggers 66 creates. A command that parses refuses with `100` a name, a frontend or an instance ending with it:

```
service name: <name>: ends with the suffix -log reserved to the loggers
```

Only the end of the whole name counts: the instance `tpl-log@x` of a frontend `tpl-log@` does not end with the suffix and is accepted, `tpl@x-log` is refused.

A frontend naming such a name in its `Depends`, `RequiredBy`, `OptsDepends`, `Provide` or `Conflict` is refused with `100` as well, the reason given at verbosity 2:

```
service: <name> names: <dep> -- the suffix -log is reserved to the loggers
```

A service parsed before 0.9.2.0 under a name ending with `-log` and that is no logger is removed from the disk by the migration to 0.9.2.0; its frontend stays. The migration acts on no supervision: such a service that runs keeps running until the next reboot, which takes it away. The migration names it, with the services that depend on it, which then name an [absent](#absent-service) service until their frontend is changed:

```
service: <name> ends with the suffix -log reserved to the loggers -- removed by the migration, parse its frontend again under another name; required by: <list>
```

A name starting with a dot is refused with `100`, for a service by every command that parses, for a tree by every command that creates one. A frontend naming such a name in its `Depends` is refused with `100` by the same rule, when the name is parsed:

```
service name: <name>: starts with a dot
tree name: <name>: starts with a dot
```

### Concurrent commands

The commands that write the system, the parse on the fly included, hold an exclusive lock of the system while they write on disk, never while they act on the supervision. A second writing command waits for the first one, and says so at verbosity 3 once the wait lasts more than a second:

```
waiting for another 66 command to release the lock of the system
```

A command that only reads the system never waits.

### Propagation

Without **-P**, a command follows the dependencies in the direction of its action:

- bringing up (`start`, `enable -S`, `signal -u/-U/-o`) follows the `Depends` of the service: what it needs comes up first;
- bringing down, signaling and reloading (`stop`, `free`, `signal` other than up, `reload`) follows the required-by dependencies: what depends on the service is acted upon first.

**-P** acts on the named services only, in the order of their dependencies, and never takes their dependencies in charge: a dependency is neither parsed, enabled, started nor stopped by it, whatever the command. It is a temporary action: the next command without **-P** runs its normal flow, whatever **-P** left behind.

Between services that do not depend on each other, no order is guaranteed.

### Running

A service runs when:

- a classic service has a living process, or is wanted up by its supervisor (a crash loop runs);
- a oneshot service is `done`;
- a module is `done`;
- a reactor is armed and supervised.

A down file (`Flags = ( down )`, `66 signal -D`) only means the service is not brought up automatically: a service started by hand with such a file runs. Every command uses this same definition.

`66 runstate` shows the current state of a service, never the one it is heading to. A service that dies by itself reads `finishing`, one brought down by `66 stop` reads `stopping`. The record of a oneshot or of a module lives under `/run` only: none is a service down, and nothing of it survives a reboot.

### Start timeout

The supervisor of a classic service with a `Notify` key gives it `[Start] Timeout` to become ready, counted from the start of its process, whatever brings it up (`66 start`, `66 signal -u`, a restart by the supervisor, the boot). Without `Notify`, a classic service is up as soon as its process starts, and nothing is timed.

The `[Start]` script of a oneshot is timed by its `[Start] Timeout` (`0`, the default, means without end): once it has passed, 66-oneshotd, its supervisor, sends `SIGTERM` to the script and to every process it forked, then `SIGKILL` once its `[Stop] Timeout` has passed (`0` means without end, no `SIGKILL`); the oneshot is down, `66 runstate` shows `result` as `timeout-start`, what waits for it is not started, and the command exits with `111` and the message above (verbosity 2).

When the time runs out before the service is ready:

- the supervisor brings it down by the way down: its down signal, then `SIGKILL` once `[Stop] Timeout` has passed;
- the service stays down and supervised, it is not started again: the next start is the admin's;
- `66 runstate` shows `result` as `timeout-start`, and the `timeout-start` event is sent;
- the command that waits for it exits with `111`, and says so at verbosity 2:

  ```
  unable to start service: <name> -- not ready within its start timeout
  ```

The way down of a service waits for its `[Stop] Timeout`, never for its `[Start] Timeout`, whatever brings it down; the supervisor sends `SIGKILL` once it has passed, 66-oneshotd for the `[Stop]` script of a oneshot and every process it forked. A `[Stop] Timeout` the frontend does not write is the general stop delay chosen at the build of 66 (`10000` milliseconds by default); `0` written means without end, no `SIGKILL`. The parse writes the delay that applies in the resolve file. A logger, which has no `[Stop]` section, gets the general stop delay.

### Absent service

A service is absent when a parsed service names it while it was never parsed, or is no longer: a `Depends` of a service started under **-P**, a `RequiredBy` naming a service never parsed, a `From` of a reactor, a removed service still named in the `RequiredBy` of a neighbour.

In the rows « absent `b` » of the tables below, `a` names `b` in its `RequiredBy`: that is what links `b` to `a` while `b` was never parsed.

- **Met through the graph**: a command that prepares (`start`, `enable`, `restart`, `reconfigure`, `status`) parses it when it reaches it through a `Depends` or a `From`, it is repaired. A `RequiredBy` is never followed by a parse. `stop`, `free`, `disable` and `remove` walk through it silently. `signal` and `reload` prepare nothing and refuse with `100`:

  ```
  service: b is not parsed -- parse it with 66 parse b
  ```

- **Named by the user**: `parse b`, `start b`, `enable b`, `status b` and `reconfigure b` parse it. `remove b` removes what depends on it, the name stays absent as long as a neighbour names it. Every other command refuses with `100` and the message above.

A name with a frontend that no parsed service names has no place in the graph: `stop`, `free` and `disable` refuse it with `100`:

```
service: <name> is not parsed -- parse it with 66 parse <name>
```

An absent service without frontend is reported at verbosity 2, and a command that has to parse it fails with `100`:

```
service: b has neither a resolve file nor a frontend -- remove it with 66 remove b
```

### Invalid service

A service is invalid when:

- its resolve file, or one of its addons, is present on disk but cannot be read: `66 reconfigure -c` repairs it;
- it is part of a dependency cycle, or depends on one: breaking the cycle in the frontends and `66 parse -f` repair it.

A service without resolve file is not invalid, it is [absent](#absent-service).

A service whose frontend is gone from every directory 66 looks in is not invalid either: the commands act from its resolve file and accept it. A command that parses it again, `66 parse -f` and `66 reconfigure`, and `66 status` naming it refuse with `100`, and `66 remove` takes it out:

```
service: b has no frontend anywhere any more -- remove it with 66 remove b
```

The dependencies a service declares are read in its own resolve file: while that file does not read, they are unknown, and the service is reached only through what its neighbours declare toward it. A command acting on one of its dependencies does not see it depend on it; `66 reconfigure -c` repairs it first.

- A command whose action touches it refuses with `100`, and nothing moves. `66 remove` and `66 reconfigure -c` accept it when it is named.
- A command whose action does not touch it acts, and warns at verbosity 2.
- When several causes apply, the message names the first of: unreadable resolve file, dependency cycle, frontend gone.
- A service on a dependency cycle that runs, named to `66 remove` or `66 reconfigure -c`, has no place in the order: it is stopped before the others.

The commands read the graph of the system from a file that 66 builds again after each command that writes the system, at the start of the scandir and after a reboot. A resolve file damaged on the disk without 66, by hand or by a fault of the disk, is seen as follows:

- A command whose action touches the service reads its resolve file, finds it unreadable, builds the graph again and refuses with `100`, as above.
- A command whose action does not touch it, run before the graph is built again, acts and says nothing about it.
- Once the graph is built again, every command names it as above.
- A dependency cycle written on the disk without 66 reads like any other resolve file: it is named and refused once the graph is built again.

An unreadable resolve file is named with the services that depend on it when there are any, alone otherwise:

```
service: b has an unreadable resolve file, and is required by: c -- repair it with 66 reconfigure -c b
service: b has an unreadable resolve file -- repair it with 66 reconfigure -c b
service: c cannot be ordered: it is part of, or depends on, a dependency cycle -- break the cycle in the frontends, then re-parse them with 66 parse -f
```

### Events

An event guarantees that the order acted on the service: a real change of state, or a signal actually delivered to its process. A oneshot sends only the event of its end, `done`, `down` or `failed` with its result, never `starting` nor `stopping`. An order that changes nothing sends no event, for every command: `66 start` or `66 signal -u`/`-o` of a classic that runs, `66 stop` or `66 signal -d` of a service already down, `66 start` of a oneshot already `done`, a signal to a service that is down. A raw signal delivered to a running service (`66 signal -k`, `-1`, `-H`...) sends the event of that signal; `66 reload` sends the reload order of the service and no signal event (see [Reactors](#reactors)).

### Reactors

- `66 start` arms a reactor `Do = start` or `restart`: its rule is registered, its `Execute` does not run until its event comes; a `service` reactor whose awaited condition already holds when it is armed fires at once; a reactor with another `Do` starts as any service.
- **Only `66 free` disarms**, whatever the type of the service, with one exception: 66-eventd disarms a reactor that fires 10 times within 10 seconds, a loop guard; `66 start` arms it again. `66 stop` and every signal leave it armed. `66 remove` and `66 reconfigure` take the service out of the supervision as `66 free` does, so they disarm it; `66 reconfigure` arms again a reactor it found armed.
- `66 restart` of a reactor brings it up now.
- A reactor armed above a restarted or reloaded service comes back armed, it is not run.
- `[Event] Propagate` defaults to `true`: `Do = X` means `66 X`, the dependencies included; `Propagate = false` carries **-P** to the action of the reactor.
- An event source (`Type = event`) has no dependencies: `66 start` arms it, nothing else.
- `Do = reload` is refused with `100` by the parse on a oneshot reactor.
- `66 reload` sends the reload order of the service and no signal event: a reactor `On = ( SIGHUP )` does not fire on a reload.
- 66 emits `boot.done` or `boot.failed` at the end of the boot, and `shutdown.begin` once the shutdown is accepted. An event name with a dot is reserved to 66: a reactor listens to it (`On = ( boot.done )`), but an `Emit` key that names one is refused with `100` by the parse.

### Loggers

- A logger is a hard dependency of its service, for every command: the service is a required-by dependency of its logger.
- A logger follows its service on `start`, `free`, `remove` and `disable`. Under **-P**, every command acts on the named services only: their logger does not follow them, except `66 reconfigure`, which parses the logger again with its service: it is freed and brought back with it.
- `66 stop` of a service leaves its logger running.
- A logger named is refused with `100` by `free`, `reconfigure` and `remove`, its service named too or not, its resolve file readable or not:

  ```
  service: <name>-log is a logger and is freed with its service -- please use '66 free <name>'
  service: <name>-log is a logger and is reconfigured with its service -- please use '66 reconfigure <name>'
  service: <name>-log is a logger and is removed with its service -- please use '66 remove <name>'
  ```

  `66 parse` refuses its name as a [reserved name](#names). Every other command acts on it.
- A logger the frontend of its service drops (`!log`) is stopped, unsupervised and removed by the parse that writes the service again. A command that brings down, signals, reloads or restarts acts on its service as well without **-P**; `66 start` of a logger starts the logger only. **-P** acts on the logger only: `66 signal -a -P foo-log` rotates the log file of `foo` and leaves `foo` untouched.

### Trees and boot

- `66 tree start` and the boot start the enabled services of a tree. An invalid service of the tree, and every service that depends on it, are left out with a warning naming them; the others start. The tree then counts as failed.
- An absent service without frontend met while preparing a tree counts as an invalid service: it and what depends on it are left out, the others start.
- A service of a tree in conflict with a running service refuses the whole tree with `100`, as [start](#start) does: nothing of the tree starts.
- `66 tree stop`, `66 tree free` and the shutdown follow the same rule: an invalid service is left out with a warning, the others stop. The tree then counts as failed.
- A service of a tree that fails follows [Failure](#failure) within its tree; a tree that fails stops only the trees that wait for it, the others start or stop.
- The boot starts the trees of the group `boot`, then the enabled trees; when a tree of the group `boot` fails, the system is not operational: the enabled trees are not started. A failure exits with `111`. The container mode is chosen by `66 boot -c` only. A tree of the group `boot` cannot be enabled.
- The shutdown frees (`66 tree free`, not `stop`) the enabled trees, then the trees of the group `boot`. It waits for each service as its stop delay says: a service written with `[Stop] Timeout = 0` holds it, the admin's choice.
- A selection of trees whose time runs out, or that receives a signal, fails with `111`.

### Modules

- A module comes with its members, under **-P** too.
- A module does not contain a module: a member of type `module` is refused with `100` by the parse.
- A module is closed: a member depends only on members of its module and watches in its `From` the members of its module only, and a service outside the module depends on no member. The module itself depends on services outside it, and is required by them, as any service. A frontend that declares a dependency or a `From` across the module is refused with `100` by the parse.
- Started, a module is marked up after its members. Stopped, it is marked down before them, and its members stop in reverse order.
- A member named alone (`module:member`) is accepted by `start`, `stop`, `free`, `signal`, `reload` and `restart`. It is refused with `100` by `parse`, `enable`, `disable`, `remove` and `reconfigure`, its module named too or not, read on the name: a member of a module never parsed is refused the same way:

  ```
  service: <m>:<x> is part of a module and cannot be <verb>d alone -- please <verb> the entire module instead using '66 <verb> <m>'
  ```
- A module one member of which was freed alone is refused with `100` by `signal`, `reload` and `restart`, and walked through by `stop` and `free`.

## start

| situation | code | effect | message |
|---|---|---|---|
| no argument | 100 | nothing | `missing service argument` |
| scandir not running | 100 | nothing, not even the parse | `scandir: <dir> is not running` |
| unknown name, alone or mixed | 100 | nothing | `service: <name> is unknown: no frontend and no service answers to the name -- is its provider enabled?` |
| never parsed | 0 | parsed with every dependency not parsed yet, in the tree of **-t**, else `InTree`, else the current tree, then started | |
| never parsed, under **-P** | 0 | the named service only is parsed and started, its `Depends` never parsed stay [absent](#absent-service) | |
| name ending with `-log`, never parsed | 100 | nothing | see [Names](#names) |
| member of a module never parsed | 100 | nothing | `service: <m>:<x> is part of a module and cannot be parsed alone -- please parse the entire module instead using '66 parse <m>'` |
| parsed, not enabled | 0 | started like any other | |
| not supervised | 0 | supervised then started | |
| classic already up | 0 | the order is sent, its supervisor keeps it up; its state does not change, no `up` event is sent and no reactor fires | `Successfully started service: <name>` |
| oneshot, module or event source already up | 0 | sent nothing | `skipping already up service: <name>` (verbosity 2) |
| classic wanted down with a living process | 0 | wanted up again | `Successfully started service: <name>` |
| `Flags = ( down )` | 0 | started | `Successfully started service: <name>` |
| dependencies | 0 | its `Depends` come up first, never what depends on it | |
| **-P** | 0 | the named services only, not their logger, whatever the state of their dependencies | |
| a service of the selection fails | 111 | what waits for it is not started | `unable to start service: <name> -- a service it waits for failed` (verbosity 2) |
| oneshot whose script fails | 111 | what waits for it is not started; `66 runstate` shows `result` as `exited:<n>` or `signaled:<SIG>` | `unable to start service: <name> -- exited with code <n>`, or `-- killed by signal <n>` (verbosity 2) |
| oneshot whose script cannot be spawned | 111 | what waits for it is not started; `66 runstate` shows `result` as `exec-failed` | `unable to start service: <name> -- unable to spawn its script: <reason>` (verbosity 2) |
| oneshot whose script runs past its `[Start] Timeout` | 111 | brought down by `SIGTERM`, `SIGKILL` once its `[Stop] Timeout` has passed, down, `66 runstate` shows `result` as `timeout-start`; what waits for it is not started | `unable to start service: <name> -- not ready within its start timeout` (verbosity 2) |
| classic with `Notify` not ready within `[Start] Timeout` | 111 | brought down by its supervisor, left down and supervised, what waits for it is not started | `unable to start service: <name> -- not ready within its start timeout` (verbosity 2), see [Start timeout](#start-timeout) |
| conflicting service running | 100 | nothing starts | `conflicting service for '<name>' -- please stop the '<other>' service first.` |
| conflicting service whose resolve file does not read | 100 | nothing starts | see [Invalid service](#invalid-service) |
| absent `b`: `start c` | 0 | `b` parsed and started, then `c` | |
| absent `b`: `start b` | 0 | `b` parsed and started | |
| absent `b`: `start a` | 0 | `b` left absent | |
| invalid service in the selection | 100 | nothing | see [Invalid service](#invalid-service) |
| module | 0 | its members in their order, the module marked up after them | |
| module of earlier services only | 0 | nothing, its members are left to the boot | `the module: <m> holds only earlier services -- nothing to act on outside the boot stage` (verbosity 2) |
| member named alone | 0 | the member only, its module is not pulled in | |
| reactor `Do = start` or `restart` | 0 | armed, its `Execute` does not run | `Armed reactor -- waiting for its event: <name>` |
| reactor with `From` | 0 | its `From` sources come up first | |
| reactor with `From`, under **-P** | 0 | its `From` sources are not pulled | |
| logger | 0 | started with its service; under **-P** it is not | |

## stop

| situation | code | effect | message |
|---|---|---|---|
| no argument | 100 | nothing | `missing service argument` |
| scandir not running | 100 | nothing | `scandir: <dir> is not running` |
| unknown name, alone or mixed | 100 | nothing | see [Names](#names) |
| frontend present, never parsed | 100 | nothing | `service: <name> is not parsed -- parse it with 66 parse <name>` |
| not supervised, alone | 0 | its required-by dependencies follow the normal flow: what runs above it stops | `skipping service without a supervisor: <name>` (verbosity 2) |
| not supervised, mixed with supervised ones | 0 | walked through at its place in the order, the others stop | `skipping service without a supervisor: <name>` (verbosity 2) |
| classic already down | 0 | the order is sent | `Successfully stopped service: <name>` |
| oneshot or module already down | 0 | sent nothing, keeps its place in the order | `skipping already down service: <name>` (verbosity 2) |
| oneshot `done` | 0 | its `[Stop]` runs once | `Successfully stopped service: <name>` |
| daemon in a crash loop | 0 | the loop stops, the service stays down and supervised | |
| required-by dependencies | 0 | what depends on it stops first; a dependent never started is left as is | |
| **-P** | 0 | the named services only, in their order | |
| absent `b`: `stop c` | 0 | `b` untouched | |
| absent `b`: `stop a` | 0 | `b` walked through, `c` then `a` stop, `b` is not parsed | |
| absent `b`: `stop b` | 100 | nothing | `service: b is not parsed -- parse it with 66 parse b` |
| invalid service in the selection | 100 | nothing | see [Invalid service](#invalid-service) |
| module | 0 | the module marked down, then its members in reverse order | |
| member named alone | 0 | the member stops, the module stays up | |
| event source armed | 0 | stays armed and supervised | `keeping armed event source, only free disarms it: <name>` (verbosity 2) |
| reactor | 0 | brought down, stays armed | |
| logger of the service | 0 | keeps running | |
| a stop fails | 111 | what it depends on is not stopped, the others stop | see [Failure](#failure) |
| a service killed by its supervisor (66-oneshotd for a oneshot) once its `[Stop] Timeout` has passed | 111 | down and supervised, what it depends on stops; `66 runstate` shows `result` as `timeout-stop` | `unable to stop service: <name> -- killed at its stop timeout` (verbosity 2) |

## free

`66 free` is `66 stop` followed by the removal of the services from the scandir. Every row of [stop](#stop) applies, with these differences:

| situation | code | effect | message |
|---|---|---|---|
| supervised | 0 | stopped then unsupervised | `Unsupervised successfully: <name>` |
| required-by dependencies | 0 | what depends on it is unsupervised too, in order | |
| absent `b`: `free a` | 0 | `b` walked through, `c` then `a` unsupervised | |
| absent `b`: `free b` | 100 | nothing | `service: b is not parsed -- parse it with 66 parse b` |
| reactor or event source | 0 | disarmed | |
| tick source, without **-P** | 0 | the reactors that declare it in their `Depends` or name it in `From` are disarmed too | |
| tick source, under **-P** | 0 | the source only | |
| source of a service reactor | 0 | the reactor stays armed | |
| module | 0 | its members unsupervised, its reactor members disarmed | |
| module with earlier members (`Flags = ( earlier )`) | 0 | its earlier members are left to the boot: neither stopped nor unsupervised | |
| member named alone | 0 | the member only, the module untouched | |
| logger of the service | 0 | unsupervised after its service | |
| logger named, its service named too or not | 100 | nothing | |
| a stop fails | 111 | it and what it depends on stay supervised, the others are unsupervised | see [Failure](#failure) |
| a service killed at its `[Stop] Timeout` | 111 | down and unsupervised, the others unsupervised | `unable to stop service: <name> -- killed at its stop timeout` (verbosity 2) |

## signal

`66 signal` is the primitive: it prepares nothing. Each option has the meaning given in [66-svctl](66-svctl.html).

| situation | code | effect | message |
|---|---|---|---|
| no argument, or no signal option | 100 | the usage | |
| invalid `-w` value, unknown or forbidden `-s` signal | 100 | nothing | `invalid value for -w -- expected one of u/U/d/D/r/R`, `invalid signal: <sig>`, `<sig> is not in the list of user-available signals` |
| scandir not running | 100 | nothing | `scandir: <dir> is not running` |
| unknown or never parsed name, alone or mixed | 100 | nothing | see [Names](#names) |
| named service not supervised, whatever the signal, `-u` included | 100 | nothing | `service: <name> is not supervised -- try to start it first` |
| direction | | `-u`, `-U`, `-o` follow the `Depends`; every other signal follows the required-by dependencies | |
| **-P** | 0 | the named services only, not their logger; a module still comes with its members | |
| classic down, `-d` or `-D` | 0 | the order is sent | `Successfully stopped service: <name>` |
| classic down, `-u`, `-U` or `-o` | 0 | brought up, its dependencies first | `Successfully started service: <name>` |
| classic down, raw signal (`-k`, `-1`, `-H`, ...) | 0 | the order is sent | `Successfully signaled service: <name>` |
| classic up, `-d` or `-D` | 0 | it and what depends on it go down and stay supervised | `Successfully stopped service: <name>` |
| classic up, raw signal | 0 | the signal reaches it and what depends on it | `Successfully signaled service: <name>` |
| classic up, `-o` | 0 | stays up, not restarted if it dies; `66 runstate` shows it up | `Successfully started service: <name>` |
| classic up, `-r` | 0 | restarted | `Successfully restarted service: <name>` |
| daemon in a crash loop, `-d` or `-D` | 0 | the loop stops, down and supervised | |
| oneshot `done`, `-d` or `-D` | 0 | its `[Stop]` runs once | `Successfully stopped service: <name>` |
| oneshot supervised and down, `-d` or `-D` | 0 | nothing, its `[Stop]` does not run again | `skipping already down service: <name>` (verbosity 2) |
| oneshot `done` or down, `-r` or raw signal | 0 | nothing | `skipping service with no process to signal: <name>` (verbosity 2) |
| oneshot `done`, `-u` | 0 | nothing | `skipping already up service: <name>` (verbosity 2) |
| oneshot supervised and down, `-u` | 0 | its `[Start]` runs | |
| dependent never started | 0 | walked through, neither supervised nor run | `skipping service without a supervisor: <name>` (verbosity 2) |
| dependent stopped by `stop -P` | 0 | left down | |
| dependency freed by `free -P`, under `-u` | 0 | walked through, not brought back | `skipping service without a supervisor: <name>` (verbosity 2) |
| absent `b` met or named | 100 | nothing, `b` is not parsed | `service: b is not parsed -- parse it with 66 parse b` |
| invalid service in the selection | 100 | nothing | see [Invalid service](#invalid-service) |
| module, `-d` | 0 | members down in reverse order, the module marked down | |
| module, raw signal | 0 | its daemon members receive it, its oneshot members nothing, the module record does not move | |
| module with a freed member | 100 | nothing, the member is not prepared again | `service: <m>:<x> is not supervised -- try to start it first` |
| event source armed, `-u`, `-U` or `-o` | 0 | stays armed | `skipping already up service: <name>` (verbosity 2) |
| event source armed, any other signal | 0 | stays armed | `skipping service with no process to signal: <name>` (verbosity 2) |
| event source not up, `-u` | 0 | armed | |
| reactor armed and idle, `-u` | 0 | armed only, its `Execute` does not run | `Armed reactor -- waiting for its event: <name>` |
| reactor running, `-d` | 0 | brought down, stays armed | |
| logger named | 0 | the logger and its service receive the signal; under **-P** the logger only | |

## reload

| situation | code | effect | message |
|---|---|---|---|
| no argument | 100 | nothing | `missing service argument` |
| scandir not running | 100 | nothing | `scandir: <dir> is not running` |
| unknown or never parsed name, alone or mixed | 100 | nothing | see [Names](#names) |
| named service not supervised | 100 | nothing, its dependents are not reloaded | `service: <name> is not supervised -- try to start it first` |
| named service supervised but not running, under **-P** too, module included | 100 | nothing | `service: <name> is not running -- start it first` |
| named service running | 0 | it and what depends on it are reloaded, never what it depends on | `Successfully reloaded service: <name>` |
| a dependency brought back up first fails | 111 | what waits for it is not reloaded, the others are | see [Failure](#failure) |
| daemon in a crash loop | 0 | reloaded | `Successfully reloaded service: <name>` |
| oneshot `done` | 0 | nothing runs again | `skipping service with no process to signal: <name>` (verbosity 2) |
| dependent never started | 0 | walked through | `skipping service without a supervisor: <name>` (verbosity 2) |
| dependent stopped by `stop -P` | 0 | left down, the others reloaded | |
| dependency left down by `stop -P` or `free -P`, needed by what runs | 0 | brought back up first, then the reload | |
| same, under **-P** | 0 | nothing brought back, the named services only | |
| absent `b` met or named | 100 | nothing, `b` is not parsed | `service: b is not parsed -- parse it with 66 parse b` |
| invalid service in the selection | 100 | nothing | see [Invalid service](#invalid-service) |
| module running | 0 | its members reloaded, the module record does not move | |
| module with a freed member | 100 | nothing | `service: <m>:<x> is not supervised -- try to start it first` |
| reactor armed above | 0 | stays armed, its `Execute` does not run | |
| event source in the selection | 0 | stays armed | `skipping service with no process to signal: <name>` (verbosity 2) |
| logger named | 0 | the logger and its service are reloaded; under **-P** the logger only | |

## restart

| situation | code | effect | message |
|---|---|---|---|
| no argument | 100 | nothing | `missing service argument` |
| scandir not running | 100 | nothing | `scandir: <dir> is not running` |
| unknown or never parsed name, alone or mixed | 100 | nothing | see [Names](#names) |
| named service not supervised, alone or mixed with supervised ones | 100 | nothing | `service: <name> is not supervised -- try to start it first` |
| named service at rest (supervised, down) | 0 | started, what depends on it and runs is stopped then brought back up; a dependent never started stays down | |
| named service running | 0 | it and what depends on it and runs are stopped, then brought back up with what they need | `Successfully stopped service: <name>`, then `Successfully started service: <name>` |
| daemon in a crash loop, named or dependent | 0 | stopped then started | |
| oneshot `done` | 0 | stopped then run again, ends `done` | |
| a stop fails | 111 | the start is sent all the same | |
| a start fails | 111 | what waits for it is not started; the reactors that do not wait for it, named or above, are armed again, none is brought up | see [Failure](#failure) |
| dependent stopped by `stop -P` | 0 | stays down | |
| dependent never started | 0 | stays down | |
| dependency left down by `stop -P` or `free -P` | 0 | brought back up and supervised | |
| **-P** | 0 | the named services only are stopped then started, in their order, a dependency never parsed is not parsed | |
| absent `b`: `restart a` | 0 | `b` parsed, started and supervised, `c` restarted | |
| absent `b`: `restart c` | 0 | `b` parsed and started, `c` restarted, `a` untouched | |
| absent `b`: `restart b` | 100 | nothing | `service: b is not parsed -- parse it with 66 parse b` |
| invalid service in the selection | 100 | nothing | see [Invalid service](#invalid-service) |
| module | 0 | its members stopped then started | |
| module with a freed member | 100 | nothing | `service: <m>:<x> is not supervised -- try to start it first` |
| reactor named, armed and idle | 0 | brought up now, under a new process | `Successfully started service: <name>` |
| reactor armed above the restarted service | 0 | comes back armed, not run | |
| event source in the selection | 0 | stays armed | `keeping armed event source, only free disarms it: <name>` (verbosity 2) |
| logger named | 0 | the logger and its service are restarted; under **-P** the logger only | |

## parse

| situation | code | effect | message |
|---|---|---|---|
| no argument | 100 | nothing | `missing service argument` |
| unknown name, alone or mixed | 100 | nothing written | `service: <name> is unknown: no frontend and no service answers to the name -- is its provider enabled?` |
| already parsed, without **-f** | 0 | nothing written | `ignoring service: <name> -- already parsed` (verbosity 2) |
| **-f** | 0 | the service alone is written again, its dependencies are not parsed again, its `Depends` is checked again; its activation and its tree do not change, **-t** aside | `Parsed successfully: <name>` |
| **-f**, supervised service whose frontend changes its type | 100 | nothing written | `service: <name> is supervised and its frontend changes its type -- free it first with 66 free <name>, or use 66 reconfigure <name>` |
| `Depends` on a name nobody answers to | 100 | nothing written | `unable to parse dependencies of service: <name>` |
| invalid frontend (type, timeout, capabilities, io path, `CopyFrom` path that does not exist...) | 100 | nothing written, every named service included | |
| name ending with `-log`, a logger included, or a frontend naming one | 100 | nothing written | see [Names](#names) |
| dependency cycle in the frontends, one or several services named | 100 | nothing written, nothing stopped | the message contains `cyclic` |
| **-f** closing a cycle with a parsed service | 100 | nothing written, nothing stopped | the message contains `cyclic` |
| dependency cycle written on the disk only | 100 | the named services written, each named service that is part of, or depends on, the cycle named | see [Invalid service](#invalid-service) |
| several services named, one of them refused | 100 | nothing written | |
| dependencies never parsed | 0 | parsed too | |
| **-f**: a `Depends` added, never parsed | 0 | the dependency is parsed, without **-f** | |
| `RequiredBy` naming a parsed service | 0 | the named service depends on the declaring one, exactly as if its own frontend declared it in its `Depends` | |
| `RequiredBy` naming a service never parsed | 0 | the declaration is kept, the named service is not parsed: a `RequiredBy` is never followed by a parse. Once it is parsed, by its name or through a `Depends`, it depends on the declaring service | |
| invalid service elsewhere | 0 | parsed, one warning per invalid service | |
| repair of an invalid service | 0 | `parse -f <name>` writes it again | |
| absent `b`: `parse b` | 0 | `b` parsed | |
| member of a module alone | 100 | nothing | `service: <m>:<x> is part of a module and cannot be parsed alone -- please parse the entire module instead using '66 parse <m>'` |
| frontend whose name is provided by an enabled provider | 100 | nothing written | `service: <name> cannot be parsed, its name is provided by: <provider> -- '66 disable -P <provider>' first` |
| frontend whose name is provided by a disabled provider | 0 | parsed, it takes its name back from the provider | |
| provider of a name an enabled service bears | 100 | nothing written | `service: <name> provides the name: <n>, which is the name of the enabled service: <n> -- '66 disable -P <n>' first` |
| provider of a name a disabled service bears | 0 | parsed, it takes the name, the service stays parsed | |
| a frontend and its provider parsed together | 100 | nothing written, in both orders | `service: <provider> provides the name: <name>, parsed along with it -- drop one of the two frontends` |
| two providers of the same name parsed together | 100 | nothing written | `service: <provider> provides the name: <n>, provided as well by: <other>, parsed along with it -- drop the name from one of the two frontends` |
| provider of a name an enabled provider holds, `-f` included | 100 | nothing written | `service: <provider> provides the name: <n>, already provided by the enabled: <holder> -- '66 disable -P <holder>' first` |
| `Provide` naming the service itself | 0 | the entry is ignored | warning `...provides its own name...` |
| **-f**: a `Provide` name dropped while a service depends on it | 100 | nothing written | `service: <provider> no longer provides the name: <n>, which service: <consumer> depends on -- remove it from the Depends of: <consumer> first` |
| **-f**: a `Provide` name dropped, nobody depends on it | 0 | the name is released | |
| **-f**: a `Depends` dropped from the frontend | 0 | the link is undone, nothing is written in the dependency | |
| **-f**: the logger dropped (`!log`) | 0 | the logger is stopped, unsupervised and removed | |
| **-f**: members dropped from a module | 0 | the dropped members are stopped, unsupervised and removed | |
| **-f**: a service dropped in a tree of the group `boot` | 0 | removed, not stopped, its supervision untouched: the change applies at the next boot | |
| **-f** refused | 100 | nothing written, nothing stopped | |

## enable

| situation | code | effect | message |
|---|---|---|---|
| no argument | 100 | nothing | `missing service argument` |
| unknown name, alone or mixed | 100 | nothing parsed, nothing enabled | `service: <name> is unknown: no frontend and no service answers to the name -- is its provider enabled?` |
| never parsed | 0 | parsed along its `Depends` and module contents only, then enabled | |
| already enabled | 0 | enabled again | `Enabled successfully: <name>` |
| dependencies | 0 | its `Depends` and loggers enabled too | |
| **-P** | 0 | the named service only, not its logger | |
| never parsed, under **-P** | 0 | the named service only is parsed and enabled, its `Depends` never parsed stay [absent](#absent-service) | |
| **-S** | 0 | enabled then started, under the normal flow of [start](#start) | |
| **-P -S** | 0 | the named service only, not its logger, is enabled then started | |
| **-S**, start refused or failing, scandir not running included | code of [start](#start) | the activation stays | |
| running service, without **-S** | 0 | not restarted | |
| **-t** *tree* | 0 | the named service and its logger move to *tree*, already parsed or not; a dependency already parsed stays in its tree, one parsed now lands in *tree* | |
| module | 0 | all its members enabled | |
| member named, its module named too or not | 100 | nothing | `service: <m>:<x> is part of a module and cannot be enabled alone -- please enable the entire module instead using '66 enable <m>'` |
| invalid service met | 100 | nothing | see [Invalid service](#invalid-service) |
| absent `b`: `enable c` | 0 | `b` parsed and enabled | |
| absent `b`: `enable a` | 0 | `b` untouched | |
| absent `b`: `enable --start b` | 0 | `b` parsed, enabled and started | |
| name provided by an enabled provider, a frontend carries the name | 100 | nothing | `name: <n> is provided by: <provider> -- '66 disable -P <provider>' first` |
| name provided by a disabled provider, a frontend carries the name | 0 | the service bearing the name is parsed and enabled, it takes its name back | |
| name provided by a disabled provider, the service bearing it already parsed | 0 | the service is enabled, it takes its name back | `Name: <n> taken back from: <provider>` (verbosity 3) |
| name provided by another service, no frontend carries it | 100 | nothing | `name: <n> is provided by: <provider> and no service has this name -- '66 enable <provider>' instead` |
| provider of a name an enabled service bears | 100 | nothing | `service: <provider> provides the name: <n>, which is the name of the enabled service: <n> -- '66 disable -P <n>' first` |
| provider of a name a disabled service bears | 0 | it takes the name, the service stays parsed | |
| provider of a name an enabled provider already holds | 100 | nothing | `service: <name> provides the name: <n>, already provided by the enabled: <other> -- '66 disable -P <other>' first` |
| provider of a free name, or of a name held by a disabled provider | 0 | it takes the name | |
| conflicting service enabled | 100 | nothing | `conflicting service for '<name>' -- please disable the '<other>' service first.` |
| conflicting service whose resolve file does not read | 100 | nothing | see [Invalid service](#invalid-service) |

## disable

| situation | code | effect | message |
|---|---|---|---|
| no argument | 100 | nothing | `missing service argument` |
| unknown name, alone or mixed | 100 | nothing | see [Names](#names) |
| frontend present, never parsed | 100 | nothing | `service: <name> is not parsed -- parse it with 66 parse <name>` |
| already disabled or never enabled | 0 | | |
| running, without **-S** | 0 | keeps running | |
| **-S** | 0 | disabled, then the named services and what depends on them are freed | |
| **-S**, a stop fails | 111 | disabled; the freeing follows [free](#free) | see [Failure](#failure) |
| **-S**, nothing supervised | 0 | disabled | `no services found to handle` |
| **-S**, scandir not running | 100 | the deactivation stays | `scandir: <dir> is not running` |
| required-by dependencies | 0 | disabled too, each before its dependency | |
| **-P** | 0 | the named service only, not its logger | |
| **-P -S** | 0 | the named service only, not its logger, is disabled then freed | |
| **-t** | | no effect | |
| module | 0 | its members disabled | |
| member named, its module named too or not | 100 | nothing | `service: <m>:<x> is part of a module and cannot be disabled alone -- please disable the entire module instead using '66 disable <m>'` |
| logger of a classic | 0 | disabled with its service | |
| invalid service met | 100 | nothing | see [Invalid service](#invalid-service) |
| absent `b`: `disable a` | 0 | `b` walked through, `c` then `a` disabled | |
| absent `b`: `disable b` | 100 | nothing | `service: b is not parsed -- parse it with 66 parse b` |
| provider answering for a name | 0 | the link stays, it still answers | |
| a provided name | 0 | its provider is disabled | |

## remove

| situation | code | effect | message |
|---|---|---|---|
| no argument | 100 | nothing | `missing service argument` |
| unknown name, alone or mixed | 100 | nothing | `service: <name> is unknown: no frontend and no service answers to the name -- is its provider enabled?` |
| never parsed name | 100 | nothing | `service: <name> is not parsed -- parse it with 66 parse <name>` |
| **-P** | 100 | nothing | `invalid option: -P`, then the usage |
| running | 0 | stopped and unsupervised before the removal, its dependents too | |
| a stop fails | 111 | nothing removed; what the command took out of the supervision is put back in the state it found it in | see [Failure](#failure) |
| tree of the boot group | 0 | not stopped, its supervision untouched, the change applies at the next boot | |
| scandir not running | 0 | removed without stopping | |
| required-by dependencies | 0 | removed with it, supervised or not | |
| its dependencies | 0 | nothing is written in them: they no longer count it among what requires them | |
| a neighbour names it in its `RequiredBy` | 0 | removed, the declaration of the neighbour stays and the name becomes [absent](#absent-service) | `service: <name> is removed, the RequiredBy of: <list> still names it` (verbosity 2) |
| absent `b`: `remove b` | 0 | `c` removed, `b` stays absent as long as a neighbour names it | `Removed successfully: b` |
| absent `b`: `remove a` | 0 | `b` walked through, `c` then `a` removed | |
| absent `b`: `remove c` | 0 | `b` untouched | |
| named service invalid | 0 | removed as far as its readable state allows; one whose resolve file does not read and that runs is stopped through its scandir entry, found by its name; a oneshot or a module has no supervisor there, it is removed without being brought down | `skipping service without a supervisor in the scandir: <name>` (verbosity 2) |
| named service invalid, its closure meets another invalid service | 100 | nothing | see [Invalid service](#invalid-service) |
| invalid service met | 100 | nothing | see [Invalid service](#invalid-service) |
| module | 0 | its members removed | |
| member named, its module named too or not | 100 | nothing | `service: <m>:<x> is part of a module and cannot be removed alone -- please remove the entire module instead using '66 remove <m>'` |
| logger named, its service named too or not | 100 | nothing | see [Loggers](#loggers) |
| service with a logger | 0 | the logger and its log directory are removed | |
| a provided name | 100 | nothing | `name: <n> is provided by: <provider> -- '66 remove <provider>' instead` |
| provider answering for a name | 0 | the name is released, nobody answers to it | |

## reconfigure

`66 reconfigure` puts the service back in the state it found it in before the reconfiguration: running or not, supervised or not, enabled or not, in its tree.

Every named frontend is checked before anything is stopped: a refusal of the parse stops nothing and writes nothing. The check parses the frontends and builds the graph, it runs no script of the service, the `configure` script of a module aside: it is part of the parse of the module, decides its members and writes in a temporary copy only. A `configure` script that fails is a failure of the system: `111`, nothing written.

| situation | code | effect | message |
|---|---|---|---|
| no argument | 100 | nothing | `missing service argument` |
| unknown name, alone or mixed | 100 | nothing parsed, nothing stopped | `service: <name> is unknown: no frontend and no service answers to the name -- is its provider enabled?` |
| invalid frontend, name ending with `-log`, dependency cycle | 100 | nothing parsed, nothing stopped | as [parse](#parse) |
| a `Depends` of a named service has neither a resolve file nor a frontend | 100 | nothing parsed, nothing stopped | as [parse](#parse): `unable to parse dependencies of service: <name>` |
| never parsed | 0 | parsed | |
| running | 0 | freed with what depends on it and is supervised, the named services parsed again with **-f**, then brought back up and enabled again if they were | `Parsed successfully: <name>` |
| reactor armed | 0 | freed, parsed again, armed again | |
| supervised, not running | 0 | parsed again, stays supervised and down | |
| not supervised | 0 | parsed again only | |
| scandir not running | 0 | parsed again and enabled again if it was | |
| tree of the boot group | 0 | parsed again only, an enabled service stays enabled | |
| only the named services are parsed again | | their dependents are not | |
| propagation | | the stop follows the required-by dependencies, the start the `Depends` | |
| **-P** | 0 | the named services only, with their logger; a dependency never parsed is not parsed | |
| a service fails to stop | 111 | nothing written; what the command took out of the supervision is put back in the state it found it in | see [Failure](#failure) |
| logger named, its service named too or not | 100 | nothing | see [Loggers](#loggers) |
| module | 0 | all its members | |
| member named, its module named too or not | 100 | nothing | `service: <m>:<x> is part of a module and cannot be reconfigured alone -- please reconfigure the entire module instead using '66 reconfigure <m>'` |
| invalid service named, without **-c** | 100 | nothing | see [Invalid service](#invalid-service) |
| invalid service met | 100 | nothing | see [Invalid service](#invalid-service) |
| invalid service elsewhere | 0 | named once | |
| **-c** | 0 | what the named services left on disk is removed, as [remove](#remove) does, their log directory and their configuration included: a cleanup starts again from nothing; their neighbours are untouched, then they are parsed again and put back in their state | |
| **-c**, unreadable addon, damaged service directory | 0 | repaired, put back in its state, its tree and its activation found again | |
| **-c**, unreadable resolve file | 0 | repaired, put back in its state and its tree; its activation cannot be read, it is left disabled | `service: <name> was enabled or not, its resolve file does not tell -- enable it again with 66 enable <name> if it was` (verbosity 2) |
| **-c**, frontend gone or name ending with `-log` | 100 | nothing | see [Invalid service](#invalid-service) |
| **-c**, tree of the boot group | 0 | not stopped, its supervision untouched, repaired on disk, the change applies at the next boot | |
| **-c** with **-P** | 0 | the named services only | |
| absent `b`: `reconfigure c` | 0 | `b` parsed, put in the state of `c` | |
| absent `b`: `reconfigure a` | 0 | `b` parsed back, `c` comes back to its state with what it needs | |
| absent `b`: `reconfigure b` | 0 | `b` parsed, left unsupervised | |
| a `Provide` or `Depends` dropped from the frontend, members dropped from a module | | as [parse](#parse) **-f** | |

## status

| situation | code | effect | message |
|---|---|---|---|
| no argument | 0 | every tree, as with **-g**; an absent service reached by a `Depends` or a `From` is parsed only | |
| no argument, **-r** | 0 | reverse order | |
| several services | 0 | each one, in the order of the graph | |
| unknown name | 100 | nothing | `service: <name> is unknown: no frontend and no service answers to the name -- is its provider enabled?` |
| never parsed | 0 | parsed, then shown | |
| dependencies never parsed | 0 | parsed along its `Depends` | |
| absent `b`: `status b` | 0 | `b` parsed, then shown | |
| named service invalid | 100 | nothing | see [Invalid service](#invalid-service) |
| named service, its frontend gone | 100 | nothing | see [Invalid service](#invalid-service) |
| invalid service elsewhere | 0 | warning | |
| scandir not running, or no status record | 0 | field `Status` is `None`, prefixed by `enabled, ` when enabled | |
| status record that does not read | 0 | field `Status` is `unreadable`, a warning names the service | |
| who changed the state | | `who` reads `user` for a command, `boot` for the boot, `event` for 66-eventd, `shutdown` for the shutdown: who wanted the current state | |
| a signal that does not change the state | | the status record does not move, neither `since` nor `who` | |

| a provided name | 0 | shows its provider | |
| module | 0 | field `contents` | |
| invalid **-d** value | 100 | nothing | `invalid depth value: <value>` |

## configure

| situation | code | effect | message |
|---|---|---|---|
| no argument | 100 | nothing | `missing service argument` |
| unknown name | 100 | nothing | `service: <name> is unknown: no frontend and no service answers to the name -- is its provider enabled?` |
| never parsed name | 100 | nothing | `service: <name> is not parsed -- parse it with 66 parse <name>` |
| several services | 0 | the first one only | a warning names each one left out |
| no configuration file | 0 | nothing | `<name> do not have configuration file` |
| version that does not exist (**-s**, **-c**) | 100 | nothing | |
| **-i** naming one version, or more than two | 100 | nothing | `import needs two versions: src,dst` |
| **-c**, the switch of the link fails | 111 | | |
| `EDITOR` not set and no **-e** | 100 | nothing | `EDITOR is not set at the environment variable -- please use the -e option to specify the editor to use e.g. 66 configure -e nano <service>.` |
| upstream file modified by hand | 100 | nothing | `invalid upstream configuration file! Do you have modified it? Tries to parse the service again.` |
| **-c** with **-s**, or two of **-V**, **-L**, **-r** | 100 | nothing | `-c and -s options are mutually exclusive`, `options -V, -L and -r are mutually exclusive` |
| member of a module | 0 | its configuration file | |
| a provided name | 0 | the configuration of its provider | |
| running service | 0 | nothing is restarted, `66 restart` applies the change | |

## env

`66 env import|set|unset|list` publishes variables of a session to every service of the scandir of its owner, in `/run/66/environment/<uid>/`, one file per variable.

| situation | code | effect | message |
|---|---|---|---|
| a variable published | 0 | merged last into the environment of a service at its start, over the frontend, the configuration and `ImportFile`; the event `env.<VAR>` is sent | |
| a variable removed | 0 | the event `unenv.<VAR>` is sent | |
| empty value, `66 env set <VAR>=` | 100 | nothing published | `empty value for variable: <VAR>` |
| empty value, `66 env import <VAR>` | 0 | the variable is ignored, nothing published | `variable: <VAR> is empty -- ignored` (verbosity 2) |
| more than 20 variables | 100 | the variable over the limit is refused by the command, nothing published, never found at the start of a service | `runtime environment directory: <dir> is full -- it can not hold more than 20 variables` |
| a value | | published as given, never checked nor guessed, a leading `!` aside | |
| a value starting with `!` | 100 | nothing published | `value of variable: <VAR> starts with an exclamation mark -- the runtime environment is published verbatim` |
| 66-eventd unreachable | 0 | the variable is written | a warning |

## log

| situation | code | effect | message |
|---|---|---|---|
| **-f** on `system` or on a service | 0 | follows its log | |
| **-f** on anything else, or with **-s** or **-u** | 100 | nothing | |

## Frontend keys

- `Notify`, `MaxDeath` and `MaxDeathInterval` belong to `[Start]`, `DownSignal` to `[Stop]`, the timeouts to the `Timeout` of `[Start]` and `[Stop]`. A key at its former place is accepted with a warning.
- `Build` is deprecated and ignored: the shebang of the script decides.
- `[Logger]` has a single `Timeout`.
- `s6log` is a deprecated alias of `66log`.

## Migration to 0.9.2.0

- A service parsed before 0.9.2.0 under a name ending with `-log` and that is no logger is removed, see [Names](#names).
- The `runas` of a logger, and the `runas` of the io of its service, left implicit by the frontend are written again as the user of the loggers chosen at the build of 66, whatever their value; a `runas` the frontend names explicitly is kept. The migration leaves the log directory and its content as they are, with the owner the previous release gave them.
- The state file of each service, and its copy under the live directory, is written again without the `toparse`, `isparsed` and `torestart` fields, which nothing read.
- The stop delay of each service is written again from its frontend: the general stop delay when `[Stop] Timeout` is not written, `0` when it is written `0`.
- The dependencies addon of each service is written again from its frontend: what 66 deduced and wrote in it before is dropped. The frontends are parsed as `66 parse` does: the `configure` script of a module runs.
