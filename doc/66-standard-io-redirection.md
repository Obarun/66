title: The 66 Suite: Standard IO redirection
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# Standard IO Redirection

The control of IO (Input/Output) redirection can be managed using the keys `StdIn`, `StdOut`, and `StdErr` within the [[Main]](66-frontend.html#section-main) section.

If none of the `StdIn`, `StdOut`, and `StdErr` keys are defined, the default behavior is to use the `s6-log` program. In this case, you can use the [[Logger]](66-frontend.html#section-logger) section to modify the behavior of `s6-log` (see the [[Logger]](66-frontend.html#section-logger) section documentation) such as log file rotation.

It should be noted that administrative permissions may be required for values `tty`, `console`, and `syslog`. A service run as a regular user will not have the necessary rights to access, for example, `tty3`.

Depending on the definition and combination of these keys, the behavior will be as follows:

## StdIn

This key allows redirection of the Standard Input (file descriptor 0) of the service. As indicated in the documentation for the [[Main]](66-frontend.html#section-main) section, this key accepts several values.

If the key is not defined, it will take the value of StdOut if StdOut is set to s6log or if StdOut is not defined at all.

### Redefinition of StdOut and StdErr based on the value of StdIn

- StdIn = tty:/path/to/tty:

    In this case, the value of StdOut will be the same as StdIn regardless of the value set for StdOut.

- StdIn = s6log

    In this case, the value of StdOut will be `s6log` and the value of StdErr will be `inherit` regardless of the values set for StdOut and StdErr.

- For the values null, parent, and close, the values of StdOut and StdErr will not be redefined.

## StdOut

This key allows redirection of the Standard Output (file descriptor 1) of the service. As indicated in the documentation for the [[Main]](66-frontend.html#section-main) section, it accepts several values.

If the key is not defined, it will take the value of StdIn as follows:

- If StdIn is set to tty or s6log, the StdOut key will take the same value tty or s6log respectively.
- If StdIn is set to null, StdOut will take the value inherit.
- If StdIn is set to parent or close, StdOut will take the value parent.

In all other cases, StdOut will take the value s6log.

This key takes precedence over StdErr depending on the chosen value.

### Redefinition of StdErr based on the value of StdOut


- If the value of StdOut is equal to StdErr, StdErr will be set to `inherit`(e.g. StdOut = tty:/dev/tty2 AND StdErr = tty:/dev/tty2).

- StdOut = syslog:

    In this case, the value of StdErr will be the same as StdOut regardless of the value set for StdErr.

- For the values tty, file, console, s6log, inherit, null, parent, and close, StdErr will not be redefined.

## StdErr

This key allows redirection of the Standard Error (file descriptor 2) of the service. As indicated in the documentation for the [[Main]](66-frontend.html#section-main) section, it accepts several values.

If the key is not defined, it will be set to inherit.

In all other cases, no redefinition is made.

## Options defined on !log

If the Options key in the [[Main]](66-frontend.html#section-main) section defines `!log`, the [[Logger]](66-frontend.html#section-logger) section will have no effect. Additionally, if the keys StdIn, StdOut, or StdErr are set to s6log or not defined at all, StdIn, StdOut, and StdErr will take the value of parent process.

# Examples

Let's take some examples:

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = s6log
    StdOut = s6log
    StdErr = inherit

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```
- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = s6log

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = s6log
    StdOut = s6log
    StdErr = inherit

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdOut = s6log

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = s6log
    StdOut = s6log
    StdErr = inherit

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = tty:/dev/tty1

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = tty:/dev/tty1
    StdOut = tty:/dev/tty1
    StdErr = inherit

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = tty:/dev/tty1
    StdOut = syslog

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = tty:/dev/tty1
    StdOut = tty:/dev/tty1
    StdErr = inherit

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = null
    StdOut = syslog

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = null
    StdOut = syslog
    StdErr = syslog

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = null

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = null
    StdOut = inherit
    StdErr = inherit

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = close

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = close
    StdOut = parent
    StdErr = inherit

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdOut = syslog

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = parent
    StdOut = syslog
    StdErr = syslog

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdOut = tty:/dev/tty1

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = parent
    StdOut = tty:/dev/tty1
    StdErr = inherit

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdOut = tty:/dev/tty1
    StdErr = file:/var/log/connman.log

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    StdIn = parent
    StdOut = tty:/dev/tty1
    StdErr = file:/var/log/connman.log

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    Options = (!log)

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    Options = (!log)
    StdIn = parent
    StdOut = parent
    StdErr = parent

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

- A frontend file with

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    Options = (!log)
    StdOut = s6log
    StdErr = file:/var/log/connmand.log

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```

    is strictly equal to

    ```
    [Main]
    Type = classic
    Description = "connman daemon"
    Version = 0.0.1
    User = ( root )
    Options = (!log)
    StdIn = parent
    StdOut = parent
    StdErr = file:/var/log/connmand.log

    [Start]
    Execute = ( connmand -n --nobacktrace --nodnsproxy )
    ```
