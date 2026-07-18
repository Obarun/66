# Upgrade and Migration Process

Starting from version `0.8.0.0`, 66 provides an internal migration process to handle upgrades when necessary. While this process mainly focuses on the [resolve files](66-deeper.html#resolve-files) (CDB databases), it may also address other components of the 66 ecosystem as required.

Distributors, system administrators, or users do not need to take any steps for the migration process, as it will be entirely transparent. 66 will automatically manage all necessary tasks within your personal 66 ecosystem to ensure the system maintains its current behavior.

**The migration process does not affect the state of running services**. Any changes will only take effect upon restarting the affected services. This also means you do not need to reboot the system to start using the new version.

Before initiating any migration, 66 creates a [snapshot](66-snapshot.html) named `system@<version>`, where `<version>` represents the current version of 66 before the new release is installed. This snapshot ensures that you can revert to the exact state of your system prior to migration by using the `66 snapshot restore system@<version>` command if something goes wrong.

Note that the prefix `system@` is reserved for snapshot names.

The migration process is triggered whenever any 66 command is executed, except for the `66 boot` and `66 snapshot` commands. This means you can create a snapshot manually without triggering the migration process if desired. The process should complete successfully. If you encounter a fatal error, [restore](66-snapshot.html#restore) your previous system and retry the migration with increased verbosity to diagnose the issue. Be sure to also downgrade any packages to match the versions used in your previous system.

The migration process only applies to the user who owns the process. Therefore, running any 66 command as root does not affect the user's 66 ecosystem. To initiate the migration for user-specific settings, you must execute a 66 command as a regular user. **It's highly recommended to trigger the migration process for the root account before doing so as a regular user**.

If a migration is required, the following general tasks will be performed, with additional tasks depending on specific migration needs:

- A snapshot is created to preserve the current state.
- The resolve files for all service trees are migrated by reading the previous files, translating them to the new format (including any necessary name changes, additions, or deletions of keys), and writing them in the updated format.
- The resolve files for all active services (i.e., those listed when running `66 tree status` with the field `contents`) are also migrated by re-parsing the services and writing them in the new format.
- The interdependence graph of services is checked and sanitized.

# Upgrade Path

`0.9.0.0` is the current target and a **mandatory** migration point: every supported system must ultimately reach it. Each release binary migrates the resolve database in a **single pass** — it carries its own migration chain and walks every intermediate step on its own, so you install the target version directly and no in-between release has to be installed by hand.

`0.9.0.0` migrates any `0.8.x` system in one step, but it no longer understands the `0.7.x` format. A system on `0.7.2.1` must therefore go through `0.8.2.2` first — which migrates every release from `0.7.2.1` up to `0.8.2.2` — and then install `0.9.0.0`.

| current version                   | target version    | mandatory version path |
| ---                               | ---               | ---            |
| between `0.7.2.1` and `0.8.2.1`   | `0.8.2.2`         | `0.8.2.2`  |
| between `0.8.0.0` and `0.8.2.2`   | `0.9.0.0`         | `0.9.0.0`  |

Releases **older than `0.7.2.1` are not supported**: neither `0.8.2.2` nor `0.9.0.0` knows their resolve format, and 66 aborts with a fatal error instead of migrating them. There is no migration path into `0.9.0.0` from any release below `0.7.2.1`.


# Version E.O.L.

Releases under `0.8.0.0` are no longer maintained — they receive no further fixes. The `0.8.2.2` binary can still migrate a `0.7.2.1` resolve as a stepping stone to `0.9.0.0`; anything older than `0.7.2.1` has no migration path at all.

| Version | E.O.L. |
| --- | --- |
| `0.8.0.0` | january 2026 |
| `0.8.1.x` to `0.8.2.2` | january 2027 |
| `0.9.0.0` | january 2028 |