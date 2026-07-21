# emit

Raise a user event by name.

## Interface

```
emit [ -h ] name
```

This command raises a user event named *name* and delivers it to the event daemon of the current [scandir](66-scandir.html). Every [reactor](66-event.html#declaring-a-reactor-the-event-section) that subscribes to that name — a service carrying an `[Event]` section with `EventType = user` and `On = ( name )` — then runs its declared reaction.

`emit` is the manual entry point into the [event system](66-event.html). The same *name* can also be raised from inside the system by another reactor's [`Emit`](66-event.html#emit) key, or by 66 itself at a system milestone such as [`boot.done` or `shutdown.begin`](66-event.html#lifecycle-events-raised-by-66); `66 emit` is the equivalent an operator or a script runs by hand. A *name* is an arbitrary label of at most 256 characters — it does not have to match a service name.

The command is fire-and-forget: it delivers the event and returns immediately. It does not wait for the reactors to act, and it reports success whether or not any reactor is currently listening for *name*. It fails only when the event daemon cannot be reached — for instance when no [scandir](66-scandir.html) is running.

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

- [the event system](66-event.html): how to declare the sources and reactors that events flow through.
