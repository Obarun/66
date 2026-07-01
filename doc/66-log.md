# log

This command displays the logs of services and of the system.

`66 log` is a read-only viewer. It locates the log destination of one service, of
the system catch-all logger, or of every service at once, reads the recorded
lines, orders them chronologically and prints them to stdout. It never writes,
rotates or alters any log.

## Interface

```
log [ -h ] [ -s since ] [ -u until ] [ -g regex ] [ -f ] service|system
```

The operand selects what to read:

- **(none)**: interleave every service logger plus the system catch-all.
- **system**: the boot/scandir catch-all logger living under the live directory.
- **service**: a single service, given by name.

In the `(none)` and `system` cases each line is tagged with the name of its
source, placed right after the timestamp and followed by `: `, in the manner of a
syslog tag. The source name is the name of the service, or `system` for the
catch-all. A single `service` operand identifies the source by itself, so its
lines are printed untagged.

## Options

- **-h, --help**: prints this help.

- **-s, --since** *time*: only show lines whose timestamp is at or after *time*.

- **-u, --until** *time*: only show lines whose timestamp is at or before *time*.

- **-g, --grep** *regex*: only show lines matching the POSIX extended regular
  expression *regex*. The expression is tested against the whole line.

- **-f, --follow**: keep running and print new lines as they are written, in the
  manner of `tail -f`. See *Follow mode* below.

## Usage example

Display every log, newest events last, all sources merged in time order:

```
$ 66 log
2026-06-30 09:12:02.114863 system: scandir up
2026-06-30 09:12:03.901221 dbus: Successfully activated service 'org.freedesktop.systemd1'
2026-06-30 09:12:04.552010 sshd: Server listening on 0.0.0.0 port 22
```

Display the log of a single service:

```
$ 66 log sshd
2026-06-30 09:12:04.552010 Server listening on 0.0.0.0 port 22
```

Restrict to a time window and filter with a regex:

```
$ 66 log -s 2026-06-30T09:00:00 -u 2026-06-30T10:00:00 -g 'error|fail' sshd
```

## Sources

A service is logged in one of two ways, set by its
[standard I/O redirection](66-standard-io-redirection.html):

- a **66-log logdir**, the usual case. `66 log` concatenates the rotated archives
  (`@<tai64n>.{s,u}`, sorted by name in ascending order) followed by the live
  `current` file, then parses the result.
- a **plain file**, when the service redirects its output to a regular file. The
  file is read as-is.

A service with no readable log destination is skipped (or, when named explicitly,
reported as an error).

The system catch-all is the logdir maintained for boot and scandir messages,
located under the live directory for the current owner.

## Timestamps and ordering

Each line carries a time key used both for the merge and for the `-s`/`-u`
filters:

- **TAI64N**-stamped lines (the format written by the 66-log loggers) are decoded
  and reprinted as **local time**, with the original `@`-prefixed stamp removed.
- **ISO 8601**-stamped lines are kept verbatim; their stamp is read as local time.
- **Unstamped** lines are printed verbatim and inherit the time key of the
  preceding line, so they stay next to the line they belong with. Having no stamp
  to sit behind, their source tag (when any) is placed at the start of the line.

When several sources are read together, their lines are merged into a single
ascending chronological stream.

## Follow mode

With `-f`, `66 log` does not print the existing history: it opens the log, skips
to the end, and then prints only the lines appended afterwards, live, until it
receives `SIGINT` or `SIGTERM`.

Because it must read a single ordered stream, `-f` is accepted **only** with an
operand — `system` or a `<service>` — never in the aggregated `(none)` form (there
is no way to interleave several live sources in true chronological order). It is
also **incompatible with `-s`/`-u`** (there is no history to bound); combining them
is an error. `-g` still applies, filtering the followed lines.

For a service logged to a **66-log logdir** (and for `system`), follow tracks the
`current` file and transparently reopens it across log rotation, so no line is
lost or duplicated when `current` is archived. For a service logged to a **plain
file**, follow tracks that file. Tagging follows the same rule as the normal
mode: `system` tags each line with `system:`, a single `<service>` is printed
untagged.

## Time format for `-s` and `-u`

*time* is given in ISO 8601, interpreted as **local time**:

- a bare date `YYYY-MM-DD` — taken at local midnight, or
- a full datetime `YYYY-MM-DDTHH:MM:SS`, with optional fractional seconds
  (`.NNN`).

The date and time are joined by `T` or `t` only; a space is rejected so the
argument never needs shell quoting. Out-of-range fields are refused.

## See also

- [logging](66-logging.html): how 66 logs services and the system.
- [standard I/O redirection](66-standard-io-redirection.html): where a service's
  output goes.
