project('66',
        'c',
        version: '0.8.2.0',
        meson_version: '>=1.1.0',
        license: 'ISC',
        default_options: [
          'c_std=c99',
          'prefix=/usr',
          'shebangdir=/usr/bin',
          'livedir=/run/66',
          'max_service=500',
          'max_path_size=1024',
          'max_service_size=256',
          'max_tree_name_size=256',
          'default_tree_name=global',
          'skeleton_dir=/etc/66',
          'system_dir=/var/lib/66',
          'system_log_dir=/var/log/66',
          's6_log_user=root',
          's6_log_timestamp=iso',
          's6_log_notification=true',
          'system_service_dir=/usr/share/66/service',
          'system_script_dir=/usr/share/66/script',
          'system_seed_dir=/usr/share/66/seed',
          'sysadmin_service_dir=/etc/66/service',
          'sysadmin_service_conf_dir=/etc/66/conf',
          'sysadmin_seed_dir=/etc/66/seed',
          'sysadmin_environment_dir=/etc/66/environment',
          'user_dir=.66',
          'user_log_dir=.66/log',
          'user_service_dir=.66/service',
          'user_service_conf_dir=.66/conf',
          'user_script_dir=.66/script',
          'user_seed_dir=.66/seed',
          'user_environment_dir=.66/environment',
          'sysdeps_dir=/usr/lib/skalibs/sysdeps',
          'enable-shared=true',
          'with_doc=false',
          'warning_level=2',
          'optimization=2',
          'buildtype=plain',
          'default_library=shared'
        ])

message('See INSTALL_MESON.md for detailed configuration instructions and warnings.')
fs = import('fs')

# Versioning
version_skalibs = '>=2.14.3.0'
version_execline = '>=2.9.6.1'
version_s6 = '>=2.13.1.0'
version_oblibs = '>=0.3.4.0'
version_lowdown = '>=0.6.4'

# Define global dependencies
skalibs_dep = dependency('skalibs', version: version_skalibs, required: true, static: get_option('enable-static'))
execline_dep = dependency('execline', version: version_execline, required: true, static: get_option('enable-static'))
s6_dep = dependency('s6', version: version_s6, required: true, static: get_option('enable-static'))
oblibs_dep = dependency('oblibs', version: version_oblibs, required: true, static: get_option('enable-static'))
doc_dep = dependency('lowdown', version: version_lowdown, required: false, static: get_option('enable-static'))

# Special case for default_options
BINDIR = get_option('prefix') / get_option('bindir')
LIBEXECDIR = get_option('prefix') / get_option('libexecdir')
INCLUDEDIR = get_option('prefix') / get_option('includedir') / '66'

# Final key value
shebangdir = get_option('shebangdir')
livedir = get_option('livedir')
max_service = get_option('max-service')
max_path_size = get_option('max-path-size')
max_service_size = get_option('max-service-size')
max_tree_name_size = get_option('max-tree-name-size')
default_tree_name = get_option('default-tree-name')
skeleton_dir = get_option('skeleton-dir')
system_dir = get_option('system-dir')
system_log_dir = get_option('system-log-dir')
s6_log_user = get_option('s6-log-user')
s6_log_timestamp = get_option('s6-log-timestamp')
s6_log_notification = get_option('s6-log-notification') ? '1' : '0'
system_service_dir = get_option('system-service-dir')
system_script_dir = get_option('system-script-dir')
system_seed_dir = get_option('system-seed-dir')
sysadmin_service_dir = get_option('sysadmin-service-dir')
sysadmin_service_conf_dir = get_option('sysadmin-service-conf-dir')
sysadmin_seed_dir = get_option('sysadmin-seed-dir')
sysadmin_environment_dir = get_option('sysadmin-environment-dir')
user_dir = get_option('user-dir')
user_log_dir = get_option('user-log-dir')
user_service_dir = get_option('user-service-dir')
user_service_conf_dir = get_option('user-service-conf-dir')
user_script_dir = get_option('user-script-dir')
user_seed_dir = get_option('user-seed-dir')
user_environment_dir = get_option('user-environment-dir')
sysdeps_dir = get_option('sysdeps-dir')

# Read sysdeps libraries
foreach lib_name : ['spawn', 'socket', 'sysclock', 'timer', 'util']
  lib_file = sysdeps_dir / lib_name + '.lib'
  if not fs.is_file(lib_file)
    warning('Sysdeps file ' + lib_file + ' does not exist')
    set_variable(lib_name + '_lib', '')
  else
    lib_content = fs.read(lib_file).strip()
    if lib_content == ''
      warning('Sysdeps file ' + lib_file + ' is empty')
      set_variable(lib_name + '_lib', '')
    else
      set_variable(lib_name + '_lib', lib_content)
    endif
  endif
endforeach

# Compiler setup
cc = meson.get_compiler('c')

# Common compiler flags
base_c_args = [
  '-pipe',
  '-fomit-frame-pointer',
  '-fno-exceptions',
  '-fno-unwind-tables',
  '-fno-asynchronous-unwind-tables',
  '-Werror=implicit-function-declaration',
  '-Werror=implicit-int',
  '-Werror=pointer-sign',
  '-Werror=pointer-arith',
  '-Wno-unused-value',
  '-Wno-parentheses',
  '-ffunction-sections',
  '-fdata-sections',
  '-D_POSIX_C_SOURCE=200809L',
  '-D_XOPEN_SOURCE=700',
  '-D_GNU_SOURCE',
  '-I' + meson.project_source_root() / 'src/include'
]

# Compiler flags
shared_c_args = base_c_args + ['-fPIC']
static_c_args = get_option('enable-all-pic') ? base_c_args + ['-fPIC'] : base_c_args
exe_c_args = base_c_args

# Linker flags
shared_link_args = [
  '-shared',
  '-Wl,--as-needed',
  '-Wl,--no-undefined',
  '-Wl,-O2',
  '-Wl,--sort-section=alignment',
  '-Wl,--sort-common'
]
if cc.has_link_argument('-Wl,--hash-style=both')
  shared_link_args += ['-Wl,--hash-style=both']
endif

static_link_args = get_option('enable-static-deps') ? ['-static', '-Wl,--gc-sections'] : []
exe_link_args = []
if get_option('enable-static-executable')
  exe_link_args += ['-static']
endif

# Add include paths
foreach inc : get_option('with-include-dir')
  if inc != '' and fs.is_dir(inc) and inc != meson.project_source_root() / 'src/include'
    add_project_arguments('-I' + inc, language: 'c')
  endif
endforeach

# Add library paths
foreach lib : get_option('with-staticlib-dir')
  if lib != '' and fs.is_dir(lib)
    shared_link_args += ['-L' + lib]
    static_link_args += ['-L' + lib]
    exe_link_args += ['-L' + lib]
  endif
endforeach
foreach dynlib : get_option('with-dynamiclib-dir')
  if dynlib != '' and fs.is_dir(dynlib)
    shared_link_args += ['-L' + dynlib]
    exe_link_args += ['-L' + dynlib]
  endif
endforeach

# Verify static libc availability if enable-static-executable is true
if get_option('enable-static-executable')
  static_libc_test = cc.find_library('c', static: true, required: false)
  if not static_libc_test.found()
    error('enable-static-executable requires a static libc, but it was not found on the system')
  endif
  # Test linking a static program
  static_link_test = cc.links('int main() { return 0; }', name: 'static executable test', args: ['-static'], dependencies: [static_libc_test])
  if not static_link_test
    error('enable-static-executable requires a toolchain capable of linking fully static executables')
  endif
endif

# Validate options to avoid conflicts
if get_option('enable-static-executable') and get_option('enable-shared')
  error('Cannot enable both enable-static-executable and enable-shared')
endif
if get_option('enable-static-deps') and get_option('enable-shared')
  error('Cannot enable both enable-static-deps and enable-shared')
endif
if get_option('enable-static-deps') and not get_option('enable-static')
  warning('enable-static-deps requires enable-static to build lib66.a; enabling enable-static')
endif
if get_option('system-service-dir') == get_option('sysadmin-service-dir')
  error('system-service-dir and sysadmin-service-dir must be different paths.')
endif
foreach file : ['user-dir', 'user-log-dir', 'user-service-dir', 'user-service-conf-dir', 'user-script-dir', 'user-seed-dir', 'user-environment-dir']
  if get_option(file).startswith('/')
    error('option', file, 'must be a relative path to $HOME, not an absolute path.')
  endif
endforeach

# Include directories
incdir = include_directories(['src/include'])

# Subdirectories
subdir('src/include/66')
subdir('src/lib66')
subdir('src/66')
subdir('skel')
subdir('doc')
