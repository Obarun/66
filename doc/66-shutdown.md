title: The 66 Suite: 66-shutdown
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# 66-shutdown

Triggers the system shutdown procedure. It is normally invoked as shutdown at your binary system directory. This program is a modified copy of [s6-linux-init-shutdown](https://skarnet.org/software/s6-linux-init/s6-linux-init-shutdown.html).

The *66-shutdown* binary is not meant to be called directly. User may prefer to use the `66 halt`, `66 poweroff` or `66 reboot` command.

## Interface

```
66-shutdown [ -H ] [ -l live ] [ -h | -p | -r | -k ] [ -a ] [ -t sec ] [ -f | -F ] time [ message ]
66-shutdown -c [ message ]
```

- Will cancel a pending shutdown if *-c* is passed.
- Else plans the shutdown procedure at time time.
- If a message argument has been given it is broadcast to all logged in users as tracked by utmp.
- Exits with `0`. The shutdown procedure happens asynchronously.

The *66-shutdown* program abides to the standards of the LSB-3.0.0 [shutdown](http://refspecs.linuxbase.org/LSB_3.0.0/LSB-PDA/LSB-PDA/shutdown.html) interface.

*time* must be one of these formats: `[ now | [+]mins | hh:mm ]` where:

- *now*: triggers the shutdown sequence immediately.

- *mins* or *+mins*: relative time; triggers the shutdown sequence after *mins* minutes.

- *hh:mm*: absolute time; triggers the shutdown sequence when the time *hh:mm* occurs. If that time has passed for the current day, it will wait for the next day. *hh* can have 1 or 2 digits; *mm* must have 2 digits.

## Options

- **-H**: prints this help.

- **-l** *live*: changes the supervision directory of *service* to *live*. By default this will be `%%livedir%%`. The default can also be changed at compile time by passing the `--livedir=live` option to `./configure`. An existing absolute path is expected and should be within a writable and executable filesystem - likely a RAM filesystem—see [66-scandir](66-scandir.html).

- **-a**: access control. The shutdown sequence will only be launched if one of the users listed in `%%skel%%/shutdown.allow` is currently logged in (as tracked by utmp). `%%skel%%/shutdown.allow` is a text file which accepts one user per line. Lines starting with `#` are commented out.

- **-t** *sec*: have a "grace time" period of sec seconds between the `SIGTERM` and the `SIGKILL` at the end of the shutdown sequence when it is time to kill all processes (allows processes to receive `SIGTERM` to exit cleanly). Default is `3` seconds.

- **-k**: do not shut down; send a warning message to all logged in users.

- **-h**: halt the system at the end of the shutdown sequence.

- **-p**: power off the system at the end of the shutdown sequence. This option is provided as an extension it is not required by the LSB interface.

- **-r**: reboot the system at the end of the shutdown sequence.

- **-f**: ignored.

- **-F**: ignored.

- **-c**: cancel a planned shutdown. Can only cancel the effect of a previous call to *shutdown* with a time argument that was not *now*. This cannot be used to interrupt a shutdown sequence that has already started.

## Notes

The **-f** and **-F** exist for compatibility reasons. The LSB sepcification says they are used to advise the system to skip or enforce a `fsck` after rebooting. But they are only advisory and for decades now systems have used other methods of evaluating whether they should perform filesystem checks so these options are largely obsolete nowadays.
