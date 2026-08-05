# Build Instructions

## Requirements

To build and install the 66 project, you need:

- A POSIX-compliant C development environment (conforming to POSIX.1-2008, available at Open Group).

- `Meson` version `1.1.0` or later: [mesonbuild.com](https://mesonbuild.com).

- `Ninja` (typically installed with Meson).

- `execline` version `2.9.6.1` or later (runtime dependency, for the generated service scripts): [skarnet.org/software/execline](https://skarnet.org/software/execline).

- `oblibs` version `0.4.0.0` or later: [git.obarun.org/Obarun/oblibs](https://git.obarun.org/Obarun/oblibs).

- `lowdown` version `0.6.4` or later (optional, for generating man pages and HTML documentation): [kristaps.bsd.lv/lowdown](https://kristaps.bsd.lv/lowdown).

- Linux API headers version 5.8 or later: [kernel.org](https://www.kernel.org).

66 targets Linux. The manager itself is written to POSIX.1-2008, but the runtime relies on
epoll, signalfd, timerfd, eventfd and inotify through `oblibs`, and the init half uses
Linux-specific interfaces; there is no BSD or macOS backend today.

## Standard Usage

For most users, the following commands will configure, build, and install the 66 project with default settings:

```bash
meson setup build -D with-doc=true
meson compile -C build
meson install -C build
```

This installs:

- Shared libraries (e.g., lib66.so) to `/usr/lib` (or the system’s library directory).
- Executables to `/usr/bin` or `/usr/libexec` (depending on the executable).
- Header files to `/usr/include/66`.
- Configuration files to `/etc/66`.
- Documentation to `/usr/share/doc/66` (HTML) and `/usr/share/man` (man pages).

To reduce binary size, you can strip symbols before installation:

```bash
meson compile -C build --strip
meson install -C build
```

Documentation (man pages and HTML) is generated and installed only if **lowdown is installed** and the `with-doc` option is enabled (default: false).

## Customization

You can customize the build using Meson options. To see all available options, run:

```bash
meson configure build
```

Example customization:

```bash
meson setup build -D prefix=/usr/local -D enable-shared=false -D enable-static=true -D enable-static-deps=true -D test=true
meson compile -C build
meson install -C build
```

## Key options include:

- `skeleton-dir`, `system-dir`, `system-log-dir`, `livedir`, and the `system-*` / `sysadmin-*` / `user-*` families: set the paths of the 66 system and user directories (e.g. `/etc/66`, `/var/lib/66`). What each of these directories holds and why it exists is documented in [deeper understanding](doc/66-deeper.md).
- `enable-shared`: Build shared libraries (e.g., `lib66.so`) for dynamic linking (default: `true`).
- `enable-static`: Build static libraries (e.g., `lib66.a`) for static linking (default: `false`).
- `enable-static-deps`: Prefer static linking for dependencies (e.g., `oblibs`) to reduce runtime dependencies; requires `-D enable-static=true` (default: `false`).
- `enable-static-executable`: Build fully static executables, including a static `libc`, for maximum portability; requires a static `libc` (e.g., `libc.a`) on the system (default: `false`).
- `enable-all-pic`: Compile static libraries with position-independent code (`PIC`) for use in shared libraries or `PIE` executables (default: `false`).
- `enable-pie`: Build executables as position-independent (`PIE`) for enhanced security via Address Space Layout Randomization (`ASLR`) (default: `false`).
- `with-doc`: Build and install man pages and HTML documentation (default: `false`).
- `doc-only`: Build the documentation only, skipping the C sources and their dependencies; requires `with-doc=true` (default: `false`).
- `test`: Build and run tests (default: `false`).
- `with-pkgconfig`: Build and install a `/usr/lib/pkgconfig/lib66.pc` file.

## Option Combinations

- You can enable both `enable-shared` and `enable-static` to build **both** shared and static libraries.
- `enable-static-deps` requires `enable-static=true` to ensure `lib66.a` is built.
- `enable-static-executable` conflicts with `enable-shared` and requires a static `libc`.
- `enable-static-deps` conflicts with `enable-shared` due to incompatible linking models.
- `enable-pie` is compatible with most options but may not work with `enable-static-executable` on some systems due to toolchain limitations.
- `enable-all-pic` applies only to static libraries and is compatible with all options.

### Important Notes on Configuration Options

Every path option below names a directory with a defined role in the 66 layout. Before
moving one, read [deeper understanding](doc/66-deeper.md): it walks the whole tree, from
`system-dir` and `livedir` down to what a resolve file is and where a service's state
lives. Choosing these paths without that picture is how installations end up subtly wrong.

- User-Related Directories: Do not set absolute paths for `user-dir`, `user-log-dir`, `user-service-dir`, `user-service-conf-dir`, `user-script-dir`, `user-seed-dir`, `user-environment-dir`. Use paths relative to `$HOME`, which will be automatically prepended. For example, `user-dir=.66` becomes `$HOME/.66`.

- Service Directories: `system-service-dir` and `sysadmin-service-dir` must be distinct paths. For example, avoid setting `sysadmin-service-dir=/etc/66/service/sysadmin` if `system-service-dir=/etc/66/service`.

- 66-log Timestamp: Valid values for `66-log-timestamp` are `tai`, `iso`, or `none`.

- Path and Service Size Limits:
  - `max-path-size`: maximum path length **in bytes** (default 1024). Each `*-dir` path, including the `$HOME` prefix, must not exceed this value.
  - `max-service-size`: maximum length of a frontend service name **in bytes** (default 256). For a service `foo@`, the runtime name (e.g. `foo@bar`) must not exceed this limit.
  - `max-tree-name-size`: same, for a tree name (default 256).
  - `max-service`: maximum number of services handled by `66-scandir` (default 500).

## Environment Variables

Meson supports a few environment variables for build customization, but passing options directly to meson setup is preferred for clarity:

- `CC`: Overrides the compiler (e.g., `CC=clang meson setup build`). When cross-compiling, the `--cross-file` option may prefix the compiler with the target triplet.

- `CFLAGS`, `CPPFLAGS`, `LDFLAGS`: Appended to Meson’s default flags. To override defaults, use Meson options or build variables instead.

## Build Variables

You can pass variables to meson compile or meson install for fine-grained control:

- `CC`, `CFLAGS`, `CPPFLAGS`, `LDFLAGS`, `LDLIBS`: Override compiler, flags, or libraries.
- `AR`, `RANLIB`, `STRIP`, `INSTALL`: Customize archiver, ranlib, strip, or install tools.
- `DESTDIR`: Specify a staging directory for installation.

Example:

```bash
CFLAGS="-O3 -march=native" meson compile -C build
DESTDIR=/tmp/staging meson install -C build
```

# Static Binaries

By default, executables are linked dynamically with `libc` and other dependencies, even if `enable-static` is used. To build fully static executables (including a static `libc`):

- Use `enable-static-executable`. This requires a static `libc` (e.g., `libc.a`) on the system, such as `libc6-dev` (Debian/Ubuntu), `glibc-static` (Fedora), or `musl-dev` (Alpine Linux).

- Note: GNU `libc` produces larger static binaries compared to alternatives like `musl`. For smaller, portable binaries, consider using `musl`.

To reduce runtime dependencies without fully static executables, use `enable-static-deps` with `enable-static=true` to link dependencies (e.g., `oblibs`) statically.

## Cross-Compilation

Cross-compilation is simplified once `oblibs` is built for the target platform.
To cross-compile:

- Create a Meson cross file (e.g., `cross-file.ini`) specifying the target triplet (e.g., `arm-linux-gnueabihf`) and toolchain paths.
- Ensure the cross-toolchain binaries (e.g., `arm-linux-gnueabihf-gcc`) are in your `PATH`.
- Customize include and library paths with `with-include-dir`, `with-staticlib-dir`, and `with-dynamiclib-dir` if needed.

Example:

```bash
meson setup build --cross-file=cross-file.ini -D with-include-dir=/path/to/target/include -D with-dynamiclib-dir=/path/to/target/lib
meson compile -C build
meson install -C build
```

## Using lib66 with pkg-config

The build generates a `lib66.pc` file for use with `with-pkgconfig`, installed to `${libdir}/pkgconfig` (e.g., `/usr/lib/pkgconfig`). To link against `lib66`:

```bash
pkg-config --cflags --libs lib66
```

## Notes

- If `enable-static-executable` is enabled, the build will fail with a clear error if a static `libc` is not found. Ensure the appropriate development package is installed (e.g., `libc6-dev`, `musl-dev`).

- If `enable-static-deps` is enabled without `enable-static`, the build will fail to ensure `lib66.a` is available.

- For security-sensitive systems, consider enabling `enable-pie` to benefit from `ASLR`.

- Documentation requires `lowdown`. If unavailable or `with-doc=false`, no documentation will be installed.
