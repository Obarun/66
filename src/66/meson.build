fs = import('fs')

exe_configs = {
  '66': {
    'sources': ['66.c'],
    'deps': [lib66_dep]
  },
  '66-echo': {
    'sources': ['66-echo.c'],
    'deps': [oblibs_dep]
  },
  '66-execute': {
    'sources': ['66-execute.c'],
    'deps': [lib66_dep]
  },
  '66-fdholderd': {
    'sources': ['66-fdholderd.c'],
    'deps': [lib66_dep]
  },
  '66-hpr': {
    'sources': ['66-hpr.c'],
    'deps': [lib66_dep]
  },
  '66-log': {
    'sources': ['66-log.c'],
    'deps': [oblibs_dep]
  },
  '66-nuke': {
    'sources': ['66-nuke.c'],
    'deps': [oblibs_dep]
  },
  '66-oneshot': {
    'sources': ['66-oneshot.c'],
    'deps': [lib66_dep]
  },
  '66-oneshotd': {
    'sources': ['66-oneshotd.c'],
    'deps': [lib66_dep]
  },
  '66-shutdown': {
    'sources': ['66-shutdown.c'],
    'deps': [lib66_dep]
  },
  '66-shutdownd': {
    'sources': ['66-shutdownd.c'],
    'deps': [lib66_dep]
  },
  '66-scandir': {
    'sources': ['66-scandir.c', 'lib/svpool.c'],
    'deps': [lib66_dep]
  },
  '66-supervise': {
    'sources': ['66-supervise.c'],
    'deps': [lib66_dep]
  },
  '66-svctl': {
    'sources': ['66-svctl.c'],
    'deps': [lib66_dep]
  },
  '66-umountall': {
    'sources': ['66-umountall.c'],
    'deps': [lib66_dep]
  },
  'execl-envfile': {
    'sources': ['execl-envfile.c'],
    'deps': [oblibs_dep]
  },
  'execl-runas': {
    'sources': ['execl-runas.c'],
    'deps': [oblibs_dep]
  }
}

libexec_exes = ['66-execute', '66-fdholderd', '66-oneshot', '66-oneshotd', '66-shutdownd', '66-svctl']
exe_install_dirs = {}
foreach exe : libexec_exes
  exe_install_dirs += {exe: get_option('libexecdir')}
endforeach

bin_dir = meson.current_source_dir()
find_dirs_cmd = run_command('sh', '-c', 'find "@0@" -maxdepth 1 -type d ! -path "@0@"'.format(bin_dir), check: false)
if find_dirs_cmd.returncode() != 0
  error('Failed to run find command for subdirectories: ' + find_dirs_cmd.stderr())
endif

exe_dirs = []
foreach dir : find_dirs_cmd.stdout().strip().split('\n')
  if dir != '' and fs.is_dir(dir)
    exe_dirs += fs.name(dir)
  endif
endforeach

if exe_dirs.length() == 0
  warning('No subdirectories found in src/66')
endif

foreach dir : exe_dirs
  exe_dir = bin_dir / dir
  foreach exe, config : exe_configs
    exe_sources = []
    foreach src : config['sources']
      src_path = exe_dir / src
      if fs.is_file(src_path)
        exe_sources += files(src_path)
      endif
    endforeach

    if exe_sources.length() == 0
      continue  # Skip if no source files found in this directory
    endif

    install_dir = exe_install_dirs.get(exe, get_option('bindir'))

    link_args = get_option('enable-static-executable') ? static_link_args : exe_link_args

    executable(exe,
      sources: exe_sources,
      include_directories: incdir,
      dependencies: config['deps'],
      install: true,
      install_dir: install_dir,
      c_args: exe_c_args,
      link_args: link_args,
      pie: get_option('enable-pie')
    )

    # Optional test
    if get_option('test')
      test_dir = exe_dir / 'test'
      if fs.is_dir(test_dir)
        # fuzz_*.c are libFuzzer harnesses (no main): they are built separately with
        # clang -fsanitize=fuzzer, not wired as meson unit tests.
        find_test_files_cmd = run_command('sh', '-c', 'find "@0@" -type f -name "*.c" ! -name "fuzz_*.c"'.format(test_dir), check: false)
        if find_test_files_cmd.returncode() != 0
          error('Failed to run find command for test files in ' + test_dir + ': ' + find_test_files_cmd.stderr())
        endif

        test_files = []
        foreach file : find_test_files_cmd.stdout().strip().split('\n')
          if file != '' and fs.is_file(file)
            test_files += files(file)
          endif
        endforeach

        if test_files.length() > 0
          test_exe = executable('test_@0@'.format(exe),
            sources: test_files,
            include_directories: incdir,
            dependencies: config['deps'],
            build_by_default: false,
            c_args: exe_c_args,
            link_args: link_args,
            pie: get_option('enable-pie')
          )
          test('@0@_test'.format(exe), test_exe)
        endif
      endif
    endif
  endforeach
endforeach

# 66-scandir ships its own data-structure lib under 66-scandir/lib (svpool,
# deliberately not part of lib66). Its unit suites each have their own main(), so the
# generic per-exe test wiring above does not fit: wire them explicitly here, one
# executable per suite, each linking the lib .c plus oblibs (for fhash_cb).
if get_option('test')
  scandir_lib_dir = bin_dir / '66-scandir' / 'lib'
  scandir_test_dir = scandir_lib_dir / 'test'
  if fs.is_dir(scandir_test_dir)
    scandir_lib_srcs = files(scandir_lib_dir / 'svpool.c')
    scandir_test_inc = include_directories('66-scandir/lib', '66-scandir/lib/test')
    scandir_link_args = get_option('enable-static-executable') ? static_link_args : exe_link_args
    foreach t : ['svpool', 'index']
      scandir_test_exe = executable('test_66-scandir_' + t,
        sources: [files(scandir_test_dir / (t + '.c'))] + scandir_lib_srcs,
        include_directories: [incdir, scandir_test_inc],
        dependencies: [oblibs_dep],
        build_by_default: false,
        c_args: exe_c_args,
        link_args: scandir_link_args,
        pie: get_option('enable-pie')
      )
      test('66-scandir_' + t + '_test', scandir_test_exe,
        env: {'LD_LIBRARY_PATH': meson.current_build_dir()})
    endforeach
  endif
endif