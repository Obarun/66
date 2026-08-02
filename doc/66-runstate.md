# runstate

This command displays the contents of the service's [*status* file](66-deeper.html).

This command is purely a debug command used by system administrator.

## Interface

```
runstate [ -h ] [ -n ] [ -f field,... ] service
```

The *status* file holds the runtime execution state of a service, as written and
kept up to date by its [66-supervise](66-supervise.html) process. Unlike
[66 state](66-state.html) — which records the *management* flags `66` uses
internally (parsed, supervised, pending actions) — `runstate` answers a different
question: is the service running right now, since when, and with which result.

The *service* argument is either a service name or an absolute path to a *status*
file.

## Options

- **-h, --help**: prints this help.

- **-n, --no-name**: do not display the field name, only its value. Useful for scripting.

- **-f, --field** *field,...*: comma separated list of fields to display.

## Usage example

Display the runtime status of service `foo`:

```
$ 66 runstate foo
state        : up
result       : success
who          : user
pid          : 1234
code         : 0
stamp        : 1782620255
readystamp   : 1782620255
window_start : 0
ndeaths      : 0
```

The output above is a service that is up, started by the user, with no recorded
death.

## Fields

| Field | Meaning |
| --- | --- |
| `state` | current execution state, see table below |
| `result` | how the service last left its running state, see table below |
| `who` | what triggered the last transition: `self`, `user`, `event`, `boot` or `shutdown` |
| `pid` | process id of the running service, `0` when no process is alive |
| `code` | last process outcome, **read according to `result`** (see below) |
| `stamp` | time the service entered its current state |
| `readystamp` | time the service became ready — transitioned to `up`, or to `done` for a `oneshot`/`module` |
| `window_start` | start of the current crash-budget window |
| `ndeaths` | number of deaths since `window_start` (crash budget) |

### `state` values

| Value | Meaning |
| --- | --- |
| `down` | the service is not running |
| `starting` | the service has been launched, not yet ready |
| `up` | the service is running (and ready) |
| `stopping` | a stop is in progress |
| `finishing` | the `finish` script is running |
| `restarting` | a restart is in progress |
| `done` | a oneshot has run to completion |
| `failed` | the crash budget is exhausted, the service is parked |
| `waiting` | an armed [event](66-event.html) reactor: it holds no process and waits for its event |

A `oneshot` or `module` reactor records `waiting` itself. A `classic` one cannot
— its record is kept by [66-supervise](66-supervise.html), which only knows
`up` and `down` — so `runstate` reports it as `down`, and it is
[66 status](66-status.html) that derives the `waiting` reading from the resolve.
Seeing `down` here on a classic reactor does not mean it is disarmed.

### `result` values

| Value | Meaning | `code` holds |
| --- | --- | --- |
| `success` | clean state, nothing to report | `0` |
| `exited` | the process exited | the exit code |
| `signaled` | the process was killed by a signal | the signal number |
| `timeout-start` | the start timeout elapsed | `0` |
| `timeout-stop` | the stop timeout elapsed | `0` |
| `crash-limit` | the crash budget was exhausted | `0` |
| `exec-failed` | the `run` script never executed | the `errno` |

`code` is only meaningful for `exited`, `signaled` and `exec-failed`; for every
other `result` it is `0`. When the crash budget is exhausted the terminal
`failed`/`crash-limit` state overrides the last death, so the exit code of the
final crash is not preserved — `ndeaths` and `result` carry the reason instead.

### Time fields

The three time fields are printed as the raw `tv_sec` of their `struct timespec`,
in seconds, with no unit conversion:

- `stamp` and `readystamp` are `CLOCK_REALTIME` values, i.e. **seconds since the
  Unix epoch** (1970-01-01 UTC). They are absolute instants: decode one with
  `date -d @<value>`. `readystamp` equals `stamp` when entering the state and
  becoming ready coincide (a service without readiness notification). On an armed
  [event](66-event.html) reactor, whose `state` returns to `waiting` after each
  firing, `readystamp` is the only field left saying **when its `Execute` last
  ran** — `0` means it has not run since it was armed.

- `window_start` is a `CLOCK_MONOTONIC` value, i.e. **seconds since boot**, not a
  date. It is only meaningful relative to the monotonic clock, as the anchor of
  the crash-budget window; do not compare it with `stamp`.

To inspect a service at a glance, prefer [66 status](66-status.html); `runstate`
is for debugging the raw runtime record, and [66 state](66-state.html) for the
internal management flags.
