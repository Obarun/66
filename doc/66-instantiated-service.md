# Instantiated services

An **instantiated** service is a frontend written once as a *template* and brought up under
many *instances*, each differing only by a name substituted in at [parse](66-parse.html) time.
One `tty@` template serves `tty@tty1`, `tty@tty2`, …; one `webapp@` [module](66-module-creation.html)
serves `webapp@blog`, `webapp@shop`, … The part after the `@` is the **instance name**.

A template is an ordinary [frontend](66-frontend.html) of any type, with two rules:

- the file name ends with an `@` (commercial at) — `tty@`;
- wherever the instance name should appear, write the [`@I`](66-identifier.html) identifier
  (any [identifier](66-identifier.html) is valid; `@I` is the instance one). It is replaced
  before the service is built.

## Example

```
File name: tty@

[Main]
Type = classic
Version = 0.0.1
Description = "Launch @I"
User = ( root )

[Start]
Execute = ( agetty -J 38400 @I )
```

Bringing up the `tty1` instance with [66 parse tty@tty1](66-parse.html) (or any other `66`
command) resolves `@I` to `tty1`:

```
[Main]
Type = classic
Version = 0.0.1
Description = "Launch tty1"
User = ( root )

[Start]
Execute = ( agetty -J 38400 tty1 )
```

## Modules are instantiated services

A [module](66-module-creation.html) is an instantiated service of `Type = module`: the same
`@`/`@I` mechanism, applied to a whole directory of services instead of a single frontend. If
you have not met instances before, read this page first, then
[creating a module](66-module-creation.html).
