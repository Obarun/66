# execl-runas

Runs a program as a given user *account*: it sets the process credentials (uid, primary gid and supplementary groups) to those of *account* and then executes *prog...*.

It is the self-contained replacement for the privilege-dropping role previously filled by [s6-setuidgid](https://skarnet.org/software/s6/s6-setuidgid.html), with a simpler and more predictable contract — see [Differences with s6-setuidgid](#differences-with-s6-setuidgid).

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

## Differences with s6-setuidgid

`execl-runas` is **not** a drop-in clone of `s6-setuidgid`. For any non-root target account the credential operations (supplementary groups, gid, uid) are identical, but the environment contract differs:

- `s6-setuidgid` is built on top of `s6-envuidgid` + `s6-applyuidgid -Uz`: the first stage *adds* `UID`/`GID`/`GIDLIST` to the environment, and the `-z` of the second stage *removes* them again before exec. As a side effect, any pre-existing `UID`/`GID`/`GIDLIST` in the environment is stripped from the program's environment.

- `execl-runas` never touches the environment. Since it resolves and applies the credentials in a single step, it never introduces `UID`/`GID`/`GIDLIST`, so there is nothing to clean up. *prog* inherits the environment unchanged.

The `user:group` colon syntax of `s6-setuidgid` is also not supported: *account* is always resolved as a single user name, and the supplementary groups are always computed from the group database.

## Usage example

```
    #!/usr/bin/execlineb -P
    redirfd -rnb 0 fifo
    execl-runas 66log
    s6-log -bpd3 -- 1 /run/66/log/0
```
