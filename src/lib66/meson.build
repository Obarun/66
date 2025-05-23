fs = import('fs')

lib66_dir = meson.current_source_dir()
find_cmd = run_command('sh', '-c', 'find "@0@" -type f -name "*.c" ! -path "*/test/*"'.format(lib66_dir), check: false)
if find_cmd.returncode() != 0
  error('Failed to run find command: ' + find_cmd.stderr())
endif

lib66_sources = []
foreach file : find_cmd.stdout().strip().split('\n')
  if file != '' and fs.is_file(file)
    lib66_sources += files(file)
  endif
endforeach

if lib66_sources.length() == 0
  warning('No .c files found in src/lib66')
endif

soversion_66 = meson.project_version()
lib66_shared = []
lib66_static = []
is_enabled = get_option('enable-shared')
is_static = get_option('enable-static') or get_option('enable-static-deps')

if is_enabled
  lib66_shared = shared_library('66',
    sources: lib66_sources,
    include_directories: incdir,
    dependencies: [
      skalibs_dep,
      oblibs_dep,
      s6_dep,
      execline_dep,
      cc.find_library(spawn_lib, required: false),
      cc.find_library(socket_lib, required: false),
      cc.find_library(sysclock_lib, required: false),
      cc.find_library(timer_lib, required: false),
      cc.find_library(util_lib, required: false)
    ],
    install: true,
    install_dir: get_option('libdir'),
    soversion: '@0@'.format(soversion_66),
    c_args: shared_c_args,
    link_args: shared_link_args,
    install_rpath: get_option('libdir'),
    build_rpath: meson.current_build_dir()
  )
endif

if is_static
  lib_static_path = join_paths(get_option('libdir'))
  lib66_static = static_library('66',
    sources: lib66_sources,
    include_directories: incdir,
    dependencies: [
      skalibs_dep,
      oblibs_dep,
      s6_dep,
      execline_dep,
      cc.find_library(spawn_lib, required: false),
      cc.find_library(socket_lib, required: false),
      cc.find_library(sysclock_lib, required: false),
      cc.find_library(timer_lib, required: false),
      cc.find_library(util_lib, required: false)
    ],
    install: true,
    install_dir: lib_static_path,
    c_args: static_c_args,
    link_args: static_link_args
  )
endif

if is_enabled == false and is_static == false
  error('At least one of enable-shared or enable-static must be true to define lib66_dep')
endif

lib66_dep = declare_dependency(
  include_directories: incdir,
  link_with: get_option('enable-shared') ? lib66_shared : lib66_static,
  dependencies: [
    skalibs_dep,
    oblibs_dep,
    s6_dep,
    execline_dep
  ]
)

header_dir = meson.project_source_root() / 'src/include/66'
find_headers_cmd = run_command('sh', '-c', 'find "@0@" -type f -name "*.h"'.format(header_dir), check: false)
if find_headers_cmd.returncode() != 0
  error('Failed to run find command for headers: ' + find_headers_cmd.stderr())
endif

header_files = []
foreach file : find_headers_cmd.stdout().strip().split('\n')
  if file != '' and fs.is_file(file)
    header_files += file
  endif
endforeach

if header_files.length() > 0
  install_headers(header_files, subdir: '66')
else
  warning('No header files found in src/include/66')
endif


if get_option('tests')
  find_test_dirs_cmd = run_command('sh', '-c', 'find "@0@" -type d -name "test"'.format(lib66_dir), check: false)
  if find_test_dirs_cmd.returncode() != 0
    error('Failed to run find command for test directories: ' + find_test_dirs_cmd.stderr())
  endif

  test_dirs = find_test_dirs_cmd.stdout().strip().split('\n')
  foreach test_dir : test_dirs
    if test_dir != '' and fs.is_dir(test_dir)
      find_test_files_cmd = run_command('sh', '-c', 'find "@0@" -type f -name "*.c"'.format(test_dir), check: false)
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
        # Extract subdirectory name for test executable
        subdir_name = test_dir.split('/').get(-2)  # Get parent directory of test/
        test_exe = executable('test_@0@'.format(subdir_name),
          sources: test_files,
          include_directories: incdir,
          dependencies: [lib66_dep],
          build_by_default: false,
          c_args: exe_c_args,
          link_args: get_option('enable-static-executable') ? static_link_args : exe_link_args,
          pie: get_option('enable-pie')
        )
        test('@0@_test'.format(subdir_name), test_exe)
      endif
    endif
  endforeach
endif