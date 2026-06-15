# 66 parser fuzzing

A libFuzzer harness over `parse_contents()`, the in-memory entry point of the
frontend parser (it splits the INI text into sections and runs every
`parse_section_*` handler), so the lexer and section parsers are exercised on
arbitrary input without touching the disk.

## Build (clang required)

```
CC=clang meson setup builddir-fuzz -Dfuzz=true -Db_sanitize=address,undefined \
    -Db_lundef=false \
    -Dwith-include-dir=/path/to/oblibs/src/include \
    -Dwith-dynamiclib-dir=/path/to/oblibs/builddir/src
meson compile -C builddir-fuzz fuzz_parse
```

`-Dfuzz=true` adds project-wide `-fsanitize=fuzzer-no-link` (coverage for lib66)
and drops `--no-undefined` from the shared link so the sanitizer runtime symbols
resolve through the driver. `b_lundef=false` is required by clang sanitizers.
The `with-*-dir` options must match the build that owns the local oblibs (same
as `builddir-asan`). Only `fuzz_parse` is compiled — the 12 programs are not
needed and link against lib66 statically.

## Run

```
builddir-fuzz/tools/fuzz/fuzz_parse work-corpus tools/fuzz/corpus \
    -max_len=8192 -print_final_stats=1
```

`corpus/` holds the committed seed inputs; pass a writable `work-corpus`
directory first so libFuzzer's generated inputs do not land in the seeds.
`ASAN_OPTIONS=detect_leaks=1` catches leaks; `UBSAN_OPTIONS=halt_on_error=1`
stops on undefined behaviour. A crash/leak is written as `crash-*` / `leak-*`
in the working directory and replayed with `fuzz_parse <file>`.
