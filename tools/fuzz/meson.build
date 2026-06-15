# libFuzzer harnesses. Enabled with -Dfuzz=true (clang only); the project-wide
# -fsanitize=fuzzer-no-link added in the root meson.build gives lib66 the
# coverage instrumentation, and -fsanitize=fuzzer here provides the driver.

fuzz_c_args = exe_c_args + ['-fsanitize=fuzzer']
fuzz_link_args = ['-fsanitize=fuzzer']

executable('fuzz_parse',
  sources: files('fuzz_parse.c'),
  include_directories: incdir,
  dependencies: [lib66_dep],
  c_args: fuzz_c_args,
  link_args: fuzz_link_args,
  build_by_default: true,
)
