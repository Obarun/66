# emit

Raise a user event by name.

## Interface

```
emit [ -h ] name
```

This command raises a user event named *name* and delivers it to the event daemon of the current [scandir](66-scandir.html). Every [reactor](66-event.html#declaring-a-reactor-the-event-section) that subscribes to that name — a service carrying an `[Event]` section with `EventType = user` and `On = ( name )` — then runs its declared reaction.

`emit` is the manual entry point into the [event system](66-event.html). The same *name* can also be raised from inside the system by another reactor's [`Emit`](66-event.html#emit) key, or by 66 itself at a system milestone; `66 emit` is the equivalent an operator or a script runs by hand. A *name* is an arbitrary label of at most 256 characters — it does not have to match a service name.

The command is fire-and-forget: it delivers the event and returns immediately. It does not wait for the reactors to act, and it reports success whether or not any reactor is currently listening for *name*. It fails only when the event daemon cannot be reached — for instance when no [scandir](66-scandir.html) is running.

## Names 66 raises itself

A `user` event does not always come from `66 emit`. 66 raises a handful of them on its own, and they all carry a **dot** — the dotted form marks an event raised by the system, while the names you raise here are bare (`cert-renewed`, `backend-down`). You subscribe to a dotted name exactly like any other, but you never emit it yourself:

| Event | Raised by |
|---|---|
| `boot.done`, `boot.failed` | [`66 boot`](66-boot.html), once every enabled tree is up — or could not be |
| `shutdown.begin` | the shutdown daemon, before any service is stopped |
| `env.<variable>` | [`66 env import`](66-env.html) and [`66 env set`](66-env.html), when *variable* becomes published with a usable value |
| `unenv.<variable>` | [`66 env unset`](66-env.html), when *variable* is no longer published |

`env.` and `unenv.` are two distinct names on purpose: a reactor is woken by a name and nothing else, so a single name for both facts would start a service on the very disappearance it was waiting to avoid. A terminal that needs a display subscribes to `env.DISPLAY` with `Do = start`, and a second service subscribes to `unenv.DISPLAY` with `Do = stop`. See [66 env](66-env.html#events) for the whole picture.

## Exit codes

- *0* success
- *100* wrong usage
- *111* system call failed

## Options

- **-h, --help**: prints this help.

## Usage examples

Raise the `cert-renewed` event from a certificate-renewal hook, so every service subscribed to that name reacts on itself:

```
66 emit cert-renewed
```

## See also

- [the event system](66-event.html): how to declare the sources and reactors that events flow through, and the [full table of the events 66 raises itself](66-event.html#lifecycle-events-raised-by-66).
- [66 env](66-env.html): publishing a variable to every service, and the `env.`/`unenv.` events it raises.
