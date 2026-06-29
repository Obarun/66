# Logging

By default, every service `66` starts gets its **own** logger — a dedicated
66-log process that captures what
the service writes and stores it, rotated and optionally timestamped, in a
human-readable file. This guide explains how that wiring works and how to tune
or disable it.

The pieces live in two places of the [frontend file](66-frontend.html): the
`Options` and `StdOut`/`StdErr` keys in `[Main]`, and the whole `[Logger]`
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

The status of a service ends with its most recent log lines. To read more
without leaving `66`, ask for the `logfile` field and a line count — this is the
native equivalent of `journalctl`:

```
66 status --field logfile --print 1000 foo
```

Or follow the underlying file directly:

```
tail -F %%system_log%%/foo/current
```

## Tuning: the [Logger] section

Optional. It controls rotation and timestamping. It also accepts `Build`,
`RunAs`, `Execute`, `TimeoutStart` and `TimeoutStop`, which behave as in
[[Start]](66-frontend.html#section-start) and [[Main]](66-frontend.html#section-main).

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
| `RunAs` | user the logger runs as | service owner |

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

## Custom loggers

With `Build = custom` in `[Logger]`, the `Backup`, `MaxSize` and `Timestamp`
keys have **no effect**: you write the logging command yourself in the
`Execute` field. Use this only when you need a logging pipeline `66-log` cannot
express.

## Where to go next

- [frontend service file](66-frontend.html#section-logger) — every `[Logger]` key.
- [Standard I/O redirection](66-standard-io-redirection.html) — where stdout/stderr can go.
- [66-status](66-status.html) — read a service's recent log inline.
