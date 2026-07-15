fs = import('fs')

file_modes = {
  'init': 'rwxr-xr-x',
  'init.conf': 'rw-r--r--',
  'rc.shutdown.final': 'rwxr-xr-x'
}

skel_dir = meson.current_source_dir()
find_files_cmd = run_command('sh', '-c', 'find "@0@" -maxdepth 1 -type f ! -name "meson.build"'.format(skel_dir), check: false)
if find_files_cmd.returncode() != 0
  error('Failed to run find command for files in skel/: ' + find_files_cmd.stderr())
endif

skel_files = []
foreach file : find_files_cmd.stdout().strip().split('\n')
  if file != '' and fs.is_file(file)
    skel_files += fs.name(file)
  endif
endforeach

if skel_files.length() == 0
  warning('No files found in skel/')
endif

foreach skel_file : skel_files
  if not file_modes.has_key(skel_file)
    warning('No mode defined for ' + skel_file + ', skipping')
    continue
  endif

  mode = file_modes[skel_file]

  # Special case for init: install to bindir
  if skel_file == 'init'
    configure_file(
      input: skel_file,
      output: skel_file,
      command: [
        'sed',
        '-e', 's|@BINDIR@|' + BINDIR + '|g',
        '-e', 's|@EXECLINE_SHEBANGPREFIX@|' + get_option('shebangdir') + '|g',
        '-e', 's|@LIVEDIR@|' + get_option('livedir') + '|g',
        '-e', 's|@SKEL@|' + get_option('skeleton-dir') + '|g',
        '@INPUT@'
      ],
      capture: true,
      install: true,
      install_dir: get_option('bindir'),
      install_mode: mode,
      install_tag: 'bin'
    )
  else
    # Other files: install to skeleton-dir
    configure_file(
      input: skel_file,
      output: skel_file,
      command: [
        'sed',
        '-e', 's|@BINDIR@|' + BINDIR + '|g',
        '-e', 's|@EXECLINE_SHEBANGPREFIX@|' + get_option('shebangdir') + '|g',
        '-e', 's|@LIVEDIR@|' + get_option('livedir') + '|g',
        '-e', 's|@SKEL@|' + get_option('skeleton-dir') + '|g',
        '@INPUT@'
      ],
      capture: true,
      install: true,
      install_dir: get_option('skeleton-dir'),
      install_mode: mode,
      install_tag: 'etc'
    )
  endif
endforeach