fs = import('fs')

ss_version = meson.project_version()

if not get_option('with-doc')
  subdir_done()
endif

# Find lowdown program (required only if doc option is enabled)
lowdown = find_program('lowdown', required: get_option('with-doc'))

# Generate HTML files
html_result = run_command('sh', 'make-html-meson.sh', ss_version, check: false)
if html_result.returncode() != 0
  warning('make-html.sh failed: ' + html_result.stderr())
endif

# Generate man pages
man_result = run_command('sh', 'make-man-meson.sh', check: false)
if man_result.returncode() != 0
  warning('make-man.sh failed: ' + man_result.stderr())
endif

# HTML
html_path = join_paths('doc', ss_version, 'html')
html_list = run_command('sh', '-c', 'find "$path" -maxdepth 1 -type f', env: ['path=' + html_path], check: false)
if html_list.returncode() != 0
  error('Failed to run find command for files in: ' + html_path + html_list.stderr())
endif

# Process all documentation files
foreach doc_html : html_list.stdout().strip().split('\n')
  if not fs.is_file(doc_html)
    warning('Documentation file ' + doc_html + ' not found')
    continue
  endif

  doc_output = doc_html.split('/').get(-1)

  # Determine installation directory
  install_dir = join_paths(get_option('datadir'), 'doc', '66', ss_version, 'html')

  # Perform sed replacements
  configure_file(
    input: doc_html,
    output: doc_output,
    command: [
      'sed',
      '-e', 's|%%shebangdir%%|' + get_option('shebangdir') + '|g',
      '-e', 's|%%skel%%|' + get_option('skeleton-dir') + '|g',
      '-e', 's|%%bindir%%|' + BINDIR + '|g',
      '-e', 's|%%livedir%%|' + get_option('livedir') + '|g',
      '-e', 's|%%system-dir%%|' + get_option('system-dir') + '|g',
      '-e', 's|%%system_log%%|' + get_option('system-log-dir') + '|g',
      '-e', 's|%%s6log_user%%|' + get_option('s6-log-user') + '|g',
      '-e', 's|%%s6log_timestamp%%|' + get_option('s6-log-timestamp') + '|g',
      '-e', 's|%%s6log_notify%%|' + (get_option('s6-log-notification') ? '0' : '1') + '|g',
      '-e', 's|%%service_system%%|' + get_option('system-service-dir') + '|g',
      '-e', 's|%%script_system%%|' + get_option('system-script-dir') + '|g',
      '-e', 's|%%seed_system%%|' + get_option('system-seed-dir') + '|g',
      '-e', 's|%%seed_adm%%|' + get_option('sysadmin-seed-dir') + '|g',
      '-e', 's|%%environment_adm%%|' + get_option('sysadmin-environment-dir') + '|g',
      '-e', 's|%%service_adm%%|' + get_option('sysadmin-service-dir') + '|g',
      '-e', 's|%%service_admconf%%|' + get_option('sysadmin-service-conf-dir') + '|g',
      '-e', 's|%%user_dir%%|' + get_option('user-dir') + '|g',
      '-e', 's|%%service_user%%|' + get_option('user-service-dir') + '|g',
      '-e', 's|%%script_user%%|' + get_option('user-script-dir') + '|g',
      '-e', 's|%%service_userconf%%|' + get_option('user-service-conf-dir') + '|g',
      '-e', 's|%%user_log%%|' + get_option('user-log-dir') + '|g',
      '-e', 's|%%seed_user%%|' + get_option('user-seed-dir') + '|g',
      '-e', 's|%%environment_user%%|' + get_option('user-environment-dir') + '|g',
      '-e', 's|%%default_treename%%|' + get_option('default-tree-name') + '|g',
      '@INPUT@'
    ],
    capture: true,
    install: true,
    install_dir: install_dir,
    install_mode: 'rw-r--r--',
    install_tag: 'html'
  )
endforeach

# MAN1
man1_path = join_paths('doc', 'man', 'man1')
man1_list = run_command('sh', '-c', 'find "$path" -maxdepth 1 -type f', env: ['path=' + man1_path], check: false)
if man1_list.returncode() != 0
  error('Failed to run find command for files in: ' + man1_path + man1_list.stderr())
endif


foreach doc_man1 : man1_list.stdout().strip().split('\n')
  if not fs.is_file(doc_man1)
    warning('Documentation file ' + doc_man1 + ' not found')
    continue
  endif

  doc_output = doc_man1.split('/').get(-1)

  install_dir = join_paths(get_option('datadir'), 'man', 'man1')

  configure_file(
    input: doc_man1,
    output: doc_output,
    command: [
      'sed',
      '-e', 's|%%shebangdir%%|' + get_option('shebangdir') + '|g',
      '-e', 's|%%skel%%|' + get_option('skeleton-dir') + '|g',
      '-e', 's|%%bindir%%|' + BINDIR + '|g',
      '-e', 's|%%livedir%%|' + get_option('livedir') + '|g',
      '-e', 's|%%system-dir%%|' + get_option('system-dir') + '|g',
      '-e', 's|%%system_log%%|' + get_option('system-log-dir') + '|g',
      '-e', 's|%%s6log_user%%|' + get_option('s6-log-user') + '|g',
      '-e', 's|%%s6log_timestamp%%|' + get_option('s6-log-timestamp') + '|g',
      '-e', 's|%%s6log_notify%%|' + (get_option('s6-log-notification') ? '0' : '1') + '|g',
      '-e', 's|%%service_system%%|' + get_option('system-service-dir') + '|g',
      '-e', 's|%%script_system%%|' + get_option('system-script-dir') + '|g',
      '-e', 's|%%seed_system%%|' + get_option('system-seed-dir') + '|g',
      '-e', 's|%%seed_adm%%|' + get_option('sysadmin-seed-dir') + '|g',
      '-e', 's|%%environment_adm%%|' + get_option('sysadmin-environment-dir') + '|g',
      '-e', 's|%%service_adm%%|' + get_option('sysadmin-service-dir') + '|g',
      '-e', 's|%%service_admconf%%|' + get_option('sysadmin-service-conf-dir') + '|g',
      '-e', 's|%%user_dir%%|' + get_option('user-dir') + '|g',
      '-e', 's|%%service_user%%|' + get_option('user-service-dir') + '|g',
      '-e', 's|%%script_user%%|' + get_option('user-script-dir') + '|g',
      '-e', 's|%%service_userconf%%|' + get_option('user-service-conf-dir') + '|g',
      '-e', 's|%%user_log%%|' + get_option('user-log-dir') + '|g',
      '-e', 's|%%seed_user%%|' + get_option('user-seed-dir') + '|g',
      '-e', 's|%%environment_user%%|' + get_option('user-environment-dir') + '|g',
      '-e', 's|%%default_treename%%|' + get_option('default-tree-name') + '|g',
      '@INPUT@'
    ],
    capture: true,
    install: true,
    install_dir: install_dir,
    install_mode: 'rw-r--r--',
    install_tag: 'man1'
  )
endforeach

# MAN5
man5_path = join_paths('doc', 'man', 'man5')
man5_list = run_command('sh', '-c', 'find "$path" -maxdepth 1 -type f', env: ['path=' + man5_path], check: false)
if man5_list.returncode() != 0
  error('Failed to run find command for files in: ' + man5_path + man5_list.stderr())
endif

foreach doc_man5 : man5_list.stdout().strip().split('\n')
  if not fs.is_file(doc_man5)
    warning('Documentation file ' + doc_man5 + ' not found')
    continue
  endif

  doc_output = doc_man5.split('/').get(-1)

  install_dir = join_paths(get_option('datadir'), 'man', 'man5')

  configure_file(
    input: doc_man5,
    output: doc_output,
    command: [
      'sed',
      '-e', 's|%%shebangdir%%|' + get_option('shebangdir') + '|g',
      '-e', 's|%%skel%%|' + get_option('skeleton-dir') + '|g',
      '-e', 's|%%bindir%%|' + BINDIR + '|g',
      '-e', 's|%%livedir%%|' + get_option('livedir') + '|g',
      '-e', 's|%%system-dir%%|' + get_option('system-dir') + '|g',
      '-e', 's|%%system_log%%|' + get_option('system-log-dir') + '|g',
      '-e', 's|%%s6log_user%%|' + get_option('s6-log-user') + '|g',
      '-e', 's|%%s6log_timestamp%%|' + get_option('s6-log-timestamp') + '|g',
      '-e', 's|%%s6log_notify%%|' + (get_option('s6-log-notification') ? '0' : '1') + '|g',
      '-e', 's|%%service_system%%|' + get_option('system-service-dir') + '|g',
      '-e', 's|%%script_system%%|' + get_option('system-script-dir') + '|g',
      '-e', 's|%%seed_system%%|' + get_option('system-seed-dir') + '|g',
      '-e', 's|%%seed_adm%%|' + get_option('sysadmin-seed-dir') + '|g',
      '-e', 's|%%environment_adm%%|' + get_option('sysadmin-environment-dir') + '|g',
      '-e', 's|%%service_adm%%|' + get_option('sysadmin-service-dir') + '|g',
      '-e', 's|%%service_admconf%%|' + get_option('sysadmin-service-conf-dir') + '|g',
      '-e', 's|%%user_dir%%|' + get_option('user-dir') + '|g',
      '-e', 's|%%service_user%%|' + get_option('user-service-dir') + '|g',
      '-e', 's|%%script_user%%|' + get_option('user-script-dir') + '|g',
      '-e', 's|%%service_userconf%%|' + get_option('user-service-conf-dir') + '|g',
      '-e', 's|%%user_log%%|' + get_option('user-log-dir') + '|g',
      '-e', 's|%%seed_user%%|' + get_option('user-seed-dir') + '|g',
      '-e', 's|%%environment_user%%|' + get_option('user-environment-dir') + '|g',
      '-e', 's|%%default_treename%%|' + get_option('default-tree-name') + '|g',
      '@INPUT@'
    ],
    capture: true,
    install: true,
    install_dir: install_dir,
    install_mode: 'rw-r--r--',
    install_tag: 'man5'
  )
endforeach

# MAN8
man8_path = join_paths('doc', 'man', 'man8')
man8_list = run_command('sh', '-c', 'find "$path" -maxdepth 1 -type f', env: ['path=' + man8_path], check: false)
if man8_list.returncode() != 0
  error('Failed to run find command for files in: ' + man8_path + man8_list.stderr())
endif

foreach doc_man8 : man8_list.stdout().strip().split('\n')
  if not fs.is_file(doc_man8)
    warning('Documentation file ' + doc_man8 + ' not found')
    continue
  endif

  doc_output = doc_man8.split('/').get(-1)

  install_dir = join_paths(get_option('datadir'), 'man', 'man8')

  configure_file(
    input: doc_man8,
    output: doc_output,
    command: [
      'sed',
      '-e', 's|%%shebangdir%%|' + get_option('shebangdir') + '|g',
      '-e', 's|%%skel%%|' + get_option('skeleton-dir') + '|g',
      '-e', 's|%%bindir%%|' + BINDIR + '|g',
      '-e', 's|%%livedir%%|' + get_option('livedir') + '|g',
      '-e', 's|%%system-dir%%|' + get_option('system-dir') + '|g',
      '-e', 's|%%system_log%%|' + get_option('system-log-dir') + '|g',
      '-e', 's|%%s6log_user%%|' + get_option('s6-log-user') + '|g',
      '-e', 's|%%s6log_timestamp%%|' + get_option('s6-log-timestamp') + '|g',
      '-e', 's|%%s6log_notify%%|' + (get_option('s6-log-notification') ? '0' : '1') + '|g',
      '-e', 's|%%service_system%%|' + get_option('system-service-dir') + '|g',
      '-e', 's|%%script_system%%|' + get_option('system-script-dir') + '|g',
      '-e', 's|%%seed_system%%|' + get_option('system-seed-dir') + '|g',
      '-e', 's|%%seed_adm%%|' + get_option('sysadmin-seed-dir') + '|g',
      '-e', 's|%%environment_adm%%|' + get_option('sysadmin-environment-dir') + '|g',
      '-e', 's|%%service_adm%%|' + get_option('sysadmin-service-dir') + '|g',
      '-e', 's|%%service_admconf%%|' + get_option('sysadmin-service-conf-dir') + '|g',
      '-e', 's|%%user_dir%%|' + get_option('user-dir') + '|g',
      '-e', 's|%%service_user%%|' + get_option('user-service-dir') + '|g',
      '-e', 's|%%script_user%%|' + get_option('user-script-dir') + '|g',
      '-e', 's|%%service_userconf%%|' + get_option('user-service-conf-dir') + '|g',
      '-e', 's|%%user_log%%|' + get_option('user-log-dir') + '|g',
      '-e', 's|%%seed_user%%|' + get_option('user-seed-dir') + '|g',
      '-e', 's|%%environment_user%%|' + get_option('user-environment-dir') + '|g',
      '-e', 's|%%default_treename%%|' + get_option('default-tree-name') + '|g',
      '@INPUT@'
    ],
    capture: true,
    install: true,
    install_dir: install_dir,
    install_mode: 'rw-r--r--',
    install_tag: 'man8'
  )
endforeach
