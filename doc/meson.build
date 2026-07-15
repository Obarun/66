if not get_option('with-doc')
  subdir_done()
endif

ss_version = meson.project_version()
lowdown = find_program('lowdown', required: true)

build_html = files('tools/build-html.sh')
build_man = files('tools/build-man.sh')
build_mkdocs = files('tools/build-mkdocs.sh')
html_header = files('html-header.md.in')

# 21 placeholder substitutions shared by all formats.
sed_args = [
  '-e', 's|%%bindir%%|@0@|g'.format(BINDIR),
  '-e', 's|%%default_treename%%|@0@|g'.format(get_option('default-tree-name')),
  '-e', 's|%%environment_adm%%|@0@|g'.format(get_option('sysadmin-environment-dir')),
  '-e', 's|%%environment_user%%|@0@|g'.format(get_option('user-environment-dir')),
  '-e', 's|%%livedir%%|@0@|g'.format(get_option('livedir')),
  '-e', 's|%%66log_user%%|@0@|g'.format(get_option('66-log-user')),
  '-e', 's|%%script_system%%|@0@|g'.format(get_option('system-script-dir')),
  '-e', 's|%%script_user%%|@0@|g'.format(get_option('user-script-dir')),
  '-e', 's|%%seed_adm%%|@0@|g'.format(get_option('sysadmin-seed-dir')),
  '-e', 's|%%seed_system%%|@0@|g'.format(get_option('system-seed-dir')),
  '-e', 's|%%seed_user%%|@0@|g'.format(get_option('user-seed-dir')),
  '-e', 's|%%service_adm%%|@0@|g'.format(get_option('sysadmin-service-dir')),
  '-e', 's|%%service_admconf%%|@0@|g'.format(get_option('sysadmin-service-conf-dir')),
  '-e', 's|%%service_system%%|@0@|g'.format(get_option('system-service-dir')),
  '-e', 's|%%service_user%%|@0@|g'.format(get_option('user-service-dir')),
  '-e', 's|%%service_userconf%%|@0@|g'.format(get_option('user-service-conf-dir')),
  '-e', 's|%%skel%%|@0@|g'.format(get_option('skeleton-dir')),
  '-e', 's|%%system_dir%%|@0@|g'.format(get_option('system-dir')),
  '-e', 's|%%system_log%%|@0@|g'.format(get_option('system-log-dir')),
  '-e', 's|%%user_dir%%|@0@|g'.format(get_option('user-dir')),
  '-e', 's|%%user_log%%|@0@|g'.format(get_option('user-log-dir')),
]

# 58 pages — HTML output (strict mirror of make-html.sh)
html_pages = [
  '66', '66-boot', '66-cheatsheet', '66-configure', '66-deeper',
  '66-dependencies', '66-disable',
  '66-echo', '66-emit', '66-enable', '66-event', '66-eventd', '66-fdholder', '66-free', '66-frontend', '66-getting-started',
  '66-halt', '66-hpr',
  '66-identifier', '66-instantiated-service', '66-log', '66-module-creation',
  '66-module-usage', '66-nuke', '66-parse', '66-poweroff', '66-reboot',
  '66-reconfigure', '66-reload', '66-remove', '66-resolve', '66-restart',
  '66-rosetta', '66-runstate', '66-scandir', '66-service-configuration-file',
  '66-shutdown', '66-shutdownd', '66-signal', '66-snapshot', '66-supervise', '66-svctl',
  '66-standard-io-redirection', '66-start', '66-state', '66-status',
  '66-stop', '66-suspend', '66-hibernate', '66-tree', '66-umountall',
  '66-upgrade', '66-upgrade-process', '66-version', '66-wall',
  '66-logging', '66-migration', '66-troubleshooting',
  'execl-envfile', 'execl-runas', 'index',
]

# 32 pages — man section 1 (strict mirror of make-man.sh)
man1_pages = [
  '66', '66-configure', '66-disable', '66-emit', '66-enable', '66-fdholder', '66-free', '66-halt',
  '66-hibernate', '66-parse', '66-poweroff', '66-reboot', '66-reconfigure',
  '66-log', '66-reload', '66-remove', '66-resolve', '66-restart', '66-rosetta',
  '66-runstate', '66-scandir', '66-signal', '66-snapshot', '66-start', '66-state',
  '66-status', '66-stop', '66-suspend', '66-tree', '66-upgrade',
  '66-version', '66-wall', 'execl-envfile', 'execl-runas',
]

# 10 pages — man section 5 (strict mirror of make-man.sh)
man5_pages = [
  '66-deeper', '66-event', '66-frontend', '66-identifier', '66-instantiated-service',
  '66-module-creation', '66-module-usage', '66-service-configuration-file',
  '66-standard-io-redirection', '66-upgrade-process',
]

# 7 pages — man section 8 (strict mirror of make-man.sh)
man8_pages = [
  '66-boot', '66-eventd', '66-hpr', '66-shutdown', '66-shutdownd', '66-supervise', '66-svctl', '66-umountall',
]

# Mkdocs uses the same set as HTML.
mkdocs_pages = html_pages

# Mkdocs needs all placeholder substitutions PLUS rewriting internal links
# from (NAME.html) to (NAME.md). Build the extended sed args once.
mkdocs_sed_args = sed_args
foreach name : html_pages
  mkdocs_sed_args += ['-e', 's|(@0@.html|(@0@.md|g'.format(name)]
endforeach

# Collected to build a single `doc` alias target. Use
# `meson compile -C build doc` to build only the documentation (HTML,
# man pages, mkdocs sources) without recompiling the C code — useful
# for the CI docbuild job.
doc_targets = []

html_install_dir = get_option('datadir') / 'doc' / '66' / ss_version / 'html'

foreach page : html_pages
  doc_targets += custom_target(
    page + '.html',
    input: page + '.md',
    output: page + '.html',
    command: [
      'sh', build_html, page, html_header, '@INPUT@', '@OUTPUT@',
    ] + sed_args,
    install: true,
    install_dir: html_install_dir,
    install_tag: 'html',
    build_by_default: true,
  )
endforeach

man1_install_dir = get_option('mandir') / 'man1'
man5_install_dir = get_option('mandir') / 'man5'
man8_install_dir = get_option('mandir') / 'man8'

foreach page : man1_pages
  doc_targets += custom_target(
    page + '.1',
    input: page + '.md',
    output: page + '.1',
    command: [
      'sh', build_man, page, '1', ss_version, '@INPUT@', '@OUTPUT@',
    ] + sed_args,
    install: true,
    install_dir: man1_install_dir,
    install_tag: 'man1',
    build_by_default: true,
  )
endforeach

foreach page : man5_pages
  doc_targets += custom_target(
    page + '.5',
    input: page + '.md',
    output: page + '.5',
    command: [
      'sh', build_man, page, '5', ss_version, '@INPUT@', '@OUTPUT@',
    ] + sed_args,
    install: true,
    install_dir: man5_install_dir,
    install_tag: 'man5',
    build_by_default: true,
  )
endforeach

foreach page : man8_pages
  doc_targets += custom_target(
    page + '.8',
    input: page + '.md',
    output: page + '.8',
    command: [
      'sh', build_man, page, '8', ss_version, '@INPUT@', '@OUTPUT@',
    ] + sed_args,
    install: true,
    install_dir: man8_install_dir,
    install_tag: 'man8',
    build_by_default: true,
  )
endforeach

subdir('mkdocs')

alias_target('doc', doc_targets)
