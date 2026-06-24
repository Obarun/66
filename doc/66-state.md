# state

This command displays the contents of the service's *state* file.

This command is purely a debug command used by system administrator or developers.

## Interface

```
state [ -h ] [ -n ] [ -f field,... ] service
```

This command displays the contents of the service's *state* file. This file are used internally by the `66` program to know runtime *service* information.

## Options

- **-h, --help**: prints this help.

- **-n, --no-name**: do not display the field name, only its value. Useful for scripting.

- **-f, --field** *field,...*: comma separated list of fields to display.

## Usage example

Display state of service `foo`:

```
$ 66 state foo
toinit        : 1
toreload      : 0
torestart     : 0
tounsupervise : 0
toparse       : 0
isparsed      : 1
issupervised  : 0
isup          : 0
```

The output above is a service that has been parsed but not started yet.

## Fields

Each field is a boolean (`1`/`0`).

| Field | Meaning |
| --- | --- |
| `isparsed` | the frontend has been parsed and a [resolve file](66-resolve.html) exists |
| `issupervised` | an `s6-supervise` process is currently managing the service |
| `isup` | the service is currently running |
| `toinit` | the service is pending initialisation in the scandir |
| `toparse` | the service is flagged to be parsed again |
| `toreload` | the service is flagged to be reloaded |
| `torestart` | the service is flagged to be restarted |
| `tounsupervise` | the service is flagged to be dropped from supervision |

The `to*` flags are transient: they record an action `66` is about to apply. To
inspect a service at a glance, prefer [66 status](66-status.html); `state` is for
debugging the internal runtime bookkeeping.
