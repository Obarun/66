# Logging

By default, every service `66` starts gets its **own** logger — a dedicated
66-log process that captures what
the service writes and stores it, rotated and optionally timestamped, in a
human-readable file. This guide explains how that wiring works and how to tune
or disable it.

The pieces live in the [frontend file](66-frontend.html): the `Options` key in
`[Main]`, the `StdOut`/`StdErr` keys in `[Execute]`, and the whole `[Logger]`
section.

## How a service's log is wired

```
   service stdout ──▶ 66-log ──▶ /var/log/66/<service>/current
                       (logger)
```

Three conditions must all hold for the automatic logger to exist (they do by
default):

1. The `log` option is **not** disabled. A logger is created unless you write
   `Options = ( !log )` in `[Main]`.
2. `StdOut` (and/or `StdIn`) is left at its default, or set explicitly to
   `66log`. Routing stdout elsewhere (a file, the console…) bypasses the logger.
3. The `[Logger]` section, if present, tunes it — it never creates it.

For a root service the log lands in `%%system_log%%/<service>/`, for a regular
user in `%%user_log%%/<service>/`. The live file is `current`; rotated files sit
beside it. File descriptors for the log pipes are kept alive by a small
fd-holder daemon, so logs survive a service restart without losing their pipe.

## Reading logs

```
66 status foo
```

The status of a service ends with its most recent log lines. To read the whole
log, use [66 log](66-log.html) — this is the native equivalent of `journalctl`:

```
66 log foo
```

It reads one service, the system catch-all (`66 log system`), or every source
interleaved in time order (`66 log` with no operand). It also follows a log as it
is written, and narrows by time or by pattern:

```
66 log --follow foo
66 log --since 2026-08-01 --grep 'fail' foo
```

Or follow the underlying file directly:

```
tail -F %%system_log%%/foo/current
```

## Tuning: the [Logger] section

Optional. It controls rotation and timestamping. It also accepts `RunAs`,
`Execute` and a single `Timeout` (the logger's start timeout — it has no stop
transition), behaving as in [[Start]](66-frontend.html#section-start). Its way down waits for
the general stop delay chosen at the build of 66, then `SIGKILL`.

```
[Logger]
Backup = 10
MaxSize = 1000000
Timestamp = iso
RunAs = user
```

| Key | Effect | Default |
| --- | --- | --- |
| `Backup` | number of rotated files kept | `3` |
| `MaxSize` | bytes before a file rotates (min `4096`) | `1000000` |
| `Timestamp` | `tai`, `iso` or `none` | `iso` |
| `RunAs` | user the logger runs as | `%%66log_user%%` for a system service, chosen at the build (`-D 66-log-user=`); the owner for a user service |

- **`tai`** prefixes each line with a TAI64N stamp (decode with
  a TAI64N timestamp converter for human-readable time).
- **`iso`** prefixes a local ISO-8601 date and time.
- **`none`** writes the line as-is.

## Turning the logger off

Set the `!log` option in `[Main]`:

```
[Main]
Type = classic
Options = ( !log )
```

The service then inherits its parent's standard output, unless you redirect it
yourself with `StdOut`. See [standard I/O redirection](66-standard-io-redirection.html)
for `file:`, `console`, `syslog`, `null` and the other targets.

Dropping the logger from the frontend of a service already parsed takes effect when the
service is parsed again, with [66 reconfigure](66-reconfigure.html) or `66 parse -f`: the
logger is stopped, unsupervised and removed.

## The logger and the commands

The logger of `foo` is named `foo-log`. It is a hard dependency of its service: `foo`
depends on `foo-log`.

- It follows its service on [start](66-start.html), [free](66-free.html),
  [remove](66-remove.html) and [disable](66-disable.html). [66 stop](66-stop.html) of the
  service leaves it running.
- Under `-P`, a command acts on the named services only: their logger does not follow them.
- Named alone, it is refused by `free`, `reconfigure` and `remove`, which handle it with its
  service. A command that brings down, signals, reloads or restarts acts on its service as well,
  except under `-P`: `66 signal -a -P foo-log` rotates the log file of `foo` and leaves `foo`
  untouched. `66 start foo-log` starts the logger only.
- The suffix `-log` is reserved to the loggers: a frontend whose name ends with it is refused
  by the parse.

## Custom loggers

With a custom (shebang) `Execute` in `[Logger]`, the `Backup`, `MaxSize` and
`Timestamp` keys have **no effect**: you write the logging command yourself in the
`Execute` field. Use this only when you need a logging pipeline `66-log` cannot
express.

## Where to go next

- [frontend service file](66-frontend.html#section-logger) — every `[Logger]` key.
- [Standard I/O redirection](66-standard-io-redirection.html) — where stdout/stderr can go.
- [66 log](66-log.html) — read, follow and filter the logs.
- [66-status](66-status.html) — read a service's recent log inline.
