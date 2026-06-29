# execl-runas

Runs a program as a given user *account*: it sets the process credentials (uid, primary gid and supplementary groups) to those of *account* and then executes *prog...*.

It is the self-contained privilege-dropping tool of the `66` suite, with a simple and predictable environment contract — see [Environment contract](#environment-contract).

## Interface

```
execl-runas account prog...
```

*account* is a user name. `execl-runas` looks it up in the user database, then:

- sets the supplementary group list to every group *account* is a member of, plus its primary group;

- sets the gid to *account*'s primary group;

- sets the uid to *account*'s uid;

- execs *prog...* with the **unchanged** environment.

The credentials are applied in the order groups, gid, uid: the uid is dropped last so that the earlier privileged operations succeed. The process must hold the privileges required by `setgroups`/`setgid`/`setuid` (typically it runs as root) — otherwise the corresponding call fails and the program exits 111.

## Options

- **-h, --help**: prints this help.

## Exit codes

- *100*: wrong usage.

- *111*: a system call failed (unknown account, `setgroups`/`setgid`/`setuid` denied, ...).

- *126*: *prog* was found but could not be executed.

- *127*: *prog* could not be found.

## Environment contract

`execl-runas` never touches the environment. It resolves and applies the credentials (supplementary groups, gid, uid) in a single step, so it never introduces or removes `UID`/`GID`/`GIDLIST` variables: there is nothing to add and nothing to clean up. *prog* inherits the environment unchanged.

There is no `user:group` colon syntax: *account* is always resolved as a single user name, and the supplementary groups are always computed from the group database.

## Usage example

```
    #!/usr/bin/execlineb -P
    redirfd -rnb 0 fifo
    execl-runas 66log
    66-log -bpd3 -- 1 /run/66/log/0
```
