title: The 66 Suite: update process
author: Eric Vidal <eric@obarun.org>

[66](index.html)

[Software](https://web.obarun.org/software)

[obarun.org](https://web.obarun.org)

# Upgrade and Migration Process

Starting from version `0.8.0.0`, 66 provides an internal migration process to handle upgrades when necessary. While this process mainly focuses on the [resolve files](66-deeper.html#resolve-files) (CDB databases), it may also address other components of the 66 ecosystem as required.

Distributors, system administrators, or users do not need to take any steps for the migration process, as it will be entirely transparent. 66 will automatically manage all necessary tasks within your personal 66 ecosystem to ensure the system maintains its current behavior.

**The migration process does not affect the state of running services**. Any changes will only take effect upon restarting the affected services. This also means you do not need to reboot the system to start using the new version.

Before initiating any migration, 66 creates a [snapshot](66-snapshot.html) named `system@<version>`, where `<version>` represents the current version of 66 before the new release is installed. This snapshot ensures that you can revert to the exact state of your system prior to migration by using the `66 snapshot restore system@<version>` command if something goes wrong.

Note that the prefix `system@` is reserved for snapshot names.

The migration process is triggered whenever any 66 command is executed, except for the `66 boot` and `66 snapshot` commands. This means you can create a snapshot manually without triggering the migration process if desired.

The migration process only applies to the user who owns the process. Therefore, running any 66 command as root does not affect the user's 66 ecosystem. To initiate the migration for user-specific settings, you must execute a 66 command as a regular user. **It's highly recommended to trigger the migration process for the root account before doing so as a regular user**.

If a migration is required, the following general tasks will be performed, with additional tasks depending on specific migration needs:

- A snapshot is created to preserve the current state.
- The resolve files for all service trees are migrated by reading the previous files, translating them to the new format (including any necessary name changes, additions, or deletions of keys), and writing them in the updated format.
- The resolve files for all active services (i.e., those listed when running `66 tree status`) are also migrated by re-parsing the services and writing them in the new format.
- The interdependence graph of services is checked and sanitized.

# Upgrade Path

Some versions may require specific intermediary versions for migration. The following table outlines the target version and any mandatory versions that must be installed along the way.

| current version | mandatory version | target version |
| --- | --- | --- |
| between 0.7.0.0 and 0.7.1.1  | 0.7.2.0 | 0.8.0.0 |

# Version E.O.L.

Releases under `0.8.0.0` are no longer supported.

| Version | E.O.L. |
| --- | --- |
| 0.8.0.0 | january 2026 |