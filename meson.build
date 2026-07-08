project('66',
        'c',
        version: '0.9.0.0',
        meson_version: '>=1.1.0',
        license: 'ISC',
        default_options: [
          'c_std=c99',
          'prefix=/usr',
          'shebangdir=/usr/bin',
          'livedir=/run/66',
          'max-service=500',
          'max-path-size=1024',
          'max-service-size=256',
          'max-tree-name-size=256',
          'default-tree-name=global',
          'skeleton-dir=/etc/66',
          'system-dir=/var/lib/66',
          'system-log-dir=/var/log/66',
          '66-log-user=root',
          '66-log-timestamp=iso',
          '66-log-notification=true',
          'system-service-dir=/usr/share/66/service',
          'system-script-dir=/usr/share/66/script',
          'system-seed-dir=/usr/share/66/seed',
          'sysadmin-service-dir=/etc/66/service',
          'sysadmin-service-conf-dir=/etc/66/conf',
          'sysadmin-seed-dir=/etc/66/seed',
          'sysadmin-environment-dir=/etc/66/environment',
          'user-dir=.66',
          'user-log-dir=.66/log',
          'user-service-dir=.66/service',
          'user-service-conf-dir=.66/conf',
          'user-script-dir=.66/script',
          'user-seed-dir=.66/seed',
          'user-environment-dir=.66/environment',
          'enable-shared=true',
          'with-doc=false',
          'with-pkgconfig=false',
          'warning_level=2',
          'optimization=2',
          'buildtype=plain',
          'default_library=shared'
        ])

message('See INSTALL.md for detailed configuration instructions and warnings.')

fs = import('fs')

# doc-only mode: build only the documentation. Skips C sources and the
# oblibs dependency lookup entirely. Useful for CI
# documentation jobs that do not need the C toolchain.
if get_option('doc-only')
  if not get_option('with-doc')
    error('doc-only=true requires with-doc=true')
  endif
  BINDIR = get_option('prefix') / get_option('bindir')
  subdir('doc')
  subdir_done()
endif

# Versioning
version_oblibs = '>=0.3.4.0'

# Compiler setup
cc = meson.get_compiler('c')

# Define dependency checks
# Common include and library paths
default_include_dirs = [get_option('prefix') / get_option('includedir')]
default_lib_dirs = [get_option('prefix') / get_option('libdir')]
user_include_dirs = get_option('with-include-dir')
user_staticlib_dirs = get_option('with-staticlib-dir')
user_dynamiclib_dirs = get_option('with-dynamiclib-dir')

# Combine default and user-provided paths
include_dirs = default_include_dirs + user_include_dirs
lib_dirs = user_staticlib_dirs + user_dynamiclib_dirs + default_lib_dirs

# Dependency: oblibs
oblibs_lib = cc.find_library('oblibs', dirs: lib_dirs, static: get_option('enable-static-deps'), required: false)
oblibs_header = cc.has_header('oblibs.h', args: ['-I', include_dirs], required: false)
if not oblibs_lib.found() or not oblibs_header
  error('oblibs ' + version_oblibs + ' not found. Ensure liboblibs and oblibs.h are installed, or specify paths with with-include-dir and with-staticlib-dir/with-dynamiclib-dir.')
endif
dep_oblibs = declare_dependency(
  dependencies: oblibs_lib,
  include_directories: include_dirs
)

# Define global dependencies
oblibs_dep = dep_oblibs

# Special case for default_options
BINDIR = get_option('prefix') / get_option('bindir')
LIBEXECDIR = get_option('prefix') / get_option('libexecdir')
INCLUDEDIR = get_option('prefix') / get_option('includedir') / '66'

# Final key value
shebangdir = get_option('shebangdir')
execline_bindir = get_option('execline-bindir')
execline_extbindir = get_option('execline-extbindir')
livedir = get_option('livedir')
max_service = get_option('max-service')
max_path_size = get_option('max-path-size')
max_service_size = get_option('max-service-size')
max_tree_name_size = get_option('max-tree-name-size')
default_tree_name = get_option('default-tree-name')
skeleton_dir = get_option('skeleton-dir')
system_dir = get_option('system-dir')
system_log_dir = get_option('system-log-dir')
log_user = get_option('66-log-user')
log_timestamp = get_option('66-log-timestamp')
log_notification = get_option('66-log-notification') ? '1' : '0'
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
  '-I' + get_option('includedir')
]

# Compiler flags
shared_c_args = base_c_args + ['-fPIC']
static_c_args = get_option('enable-all-pic') ? base_c_args + ['-fPIC'] : base_c_args
exe_c_args = base_c_args

# Linker flags
shared_link_args = [
  '-shared',
  '-Wl,--as-needed',
  '-Wl,-O2',
  '-Wl,--sort-section=alignment',
  '-Wl,--sort-common'
]

shared_link_args += ['-Wl,--no-undefined']

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
  error('enable-static-deps requires enable-static to build lib66.a')
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

if get_option('with-pkgconfig')
  # Generate and install pkg-config file for lib66
  private_libs = []
  if get_option('enable-static') or get_option('enable-static-deps')
    private_libs += ['-loblibs']
  endif

  pkg = import('pkgconfig')
  pkg.generate(
    name: 'lib66',
    filebase: 'lib66',
    description: 'Library for the 66 service management system',
    version: meson.project_version(),
    libraries: ['-l66'],
    libraries_private: private_libs,
    subdirs: ['66'], # Headers in ${includedir}/66
  )
endif