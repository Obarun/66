# Troubleshooting & FAQ

When a service misbehaves, `66` almost always has the answer in `66 status` or
in the service's log. This page collects the most common situations and the
command that diagnoses each.

## First reflexes

```
66 status foo                    # state, type, source, and the last log lines
66 --verbosity 2 parse foo       # re-parse and surface frontend errors and warnings
tail -F %%system_log%%/foo/current   # the full log
```

Raise the verbosity when diagnosing: the default level only prints errors, so
**`--verbosity 2` (or higher) is the minimum to see warnings** — and many
frontend problems are reported as warnings, not hard errors.

The `Status` line of `66 status` is the single most useful field: it tells you
whether the service is `up`, `down`, `crashed`, enabled or disabled.

## My service won't start

- **Is the scandir running?** `start` needs a live [scandir](66-scandir.html).
  It is normally brought up by `66-userd` at boot, but if yours is not, run
  `66 scandir start` (idempotent: it returns at once if one is
  already up, otherwise it launches the supervisor in the foreground — background
  it with `&` or use a dedicated terminal). For root, the boot scandir is
  normally already up.
- **Did it parse?** Run `66 --verbosity 2 parse foo` (verbosity `2` or higher,
  so warnings show). A frontend problem (empty value, a section before `[Main]`,
  a malformed bracket list) stops the service from ever being built. See
  [frontend service file](66-frontend.html).
- **Are you allowed to manage it?** The `User` key in `[Main]` lists who may
  operate the service. Note that **`root` is not implicit** — if `User` does not
  include `root`, even root cannot manage it.

## My service starts, then immediately dies (and restarts in a loop)

This is the classic supervision pitfall: **a supervised service must run in the
foreground.** If your daemon forks into the background (daemonizes), `66`
sees the foreground process exit and treats it as a crash, then restarts it —
forever.

Pass whatever flag keeps the program in the foreground:

```
Execute = ( /usr/bin/mydaemon --foreground )   # or -d, --nofork, -f, etc.
```

This is why the examples across the docs use `dbus-daemon --nofork`,
`ntpd -d`, `auditd -f`, and so on.

## I changed the frontend file but nothing changed

Editing a frontend file does **not** re-apply it. The service runs from its
parsed form. Re-apply with:

```
66 reconfigure foo
```

which stops, unsupervises, re-parses and restarts the service in one step.

## I changed an environment value but the service ignores it

Configuration is versioned. After `66 configure foo`, restart the service to
pick it up:

```
66 restart foo
```

See [service configuration file](66-service-configuration-file.html).

## enable vs start — which do I need?

- `66 start foo` runs it **now**, but it will not come back after a reboot.
- `66 enable foo` registers it for **future** boots, but does not run it now.
- `66 enable --start foo` does both.

A service can be `up` but `disabled` (started by hand, won't survive reboot), or
`down` but `enabled` (will come up next boot). The `Status` field shows both.

## I can't remove or re-create a service / scandir

- `66 remove foo` is irreversible and removes everything except your frontend
  file. If a service is running, stop it first.
- `66 scandir create` refuses to overwrite an existing scandir — `66 scandir
  remove` it first (after `66 scandir stop`).

## Reading more than the tail of a log

`66 status foo` shows only the last few lines; [66 log](66-log.html) reads the
whole thing:

```
66 log foo                       # everything this service logged
66 log --follow foo              # and keep printing as it runs
66 log --grep 'fail' foo         # only the lines that match
```

The full, rotated history also lives on disk in `%%system_log%%/foo/` (root) or
`%%user_log%%/foo/` (regular user). If the log is TAI64N-stamped, pipe it
through a TAI64N timestamp converter for readable timestamps.

## Where to go next

- [66 log](66-log.html) — read, follow and filter the logs.
- [66-status](66-status.html) — every field it can show.
- [Logging](66-logging.html) — how the per-service logger is wired.
- [66-parse](66-parse.html) — inspect what the frontend compiled to.
