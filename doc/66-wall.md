title: The 66 Suite: wall
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# wall

Send a message to all users connected to a tty.

## Interface

```
wall [ -h ] message
```

This command sends a *message* to all connected users. It uses `UTMP` to find out which users are logged in. If the *message* contains spaces, it must be enclosed in quotes.

## Exit codes

- *0* success
- *100* wrong usage
- *111* system call failed

## Options

- **-h**: prints this help;

## Usage examples

Send a message to all connected users

```
66 wall "The system will be shut down in 5 minutes. Please, save your work and log out."
```