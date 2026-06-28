fs = import('fs')

# Validate 66-log timestamp
if log_timestamp == 'tai'
  log_timestamp = '0'
elif log_timestamp == 'iso'
  log_timestamp = '1'
elif log_timestamp == 'none'
  log_timestamp = '2'
else
  error('Invalid 66-log-timestamp value. Valid values are: tai, iso, none')
endif

# Validate integer options
foreach opt_name : ['max-service', 'max-path-size', 'max-service-size', 'max-tree-name-size']
  opt_int = get_option(opt_name)
  if opt_int < 2
    error('Value for ' + opt_name + ' must be at least 2, got: ' + opt_int.to_string())
  endif
endforeach

# Validate default tree name length
name_length = run_command('sh', '-c', 'printf "%s" "${#1}"', default_tree_name, check: true).stdout().strip().to_int()
if name_length > max_tree_name_size
  error('Maximum length of default-tree-name is ' + max_tree_name_size.to_string() + ', got: ' + default_tree_name)
endif

# Validate path lengths
foreach path : [skeleton_dir, system_dir, system_log_dir, system_service_dir, system_script_dir, system_seed_dir,
                sysadmin_service_dir, sysadmin_service_conf_dir, sysadmin_seed_dir, sysadmin_environment_dir,
                user_dir, user_log_dir, user_service_dir, user_service_conf_dir, user_script_dir, user_seed_dir, user_environment_dir]
  path_length = run_command('sh', '-c', 'printf "%s" "${#1}"', path, check: true).stdout().strip().to_int()
  if path_length >= max_path_size
    error('Path length for ' + path + ' exceeds max-path-size of ' + max_path_size.to_string())
  endif
endforeach

conf = configuration_data()
conf.set_quoted('SS_VERSION', meson.project_version())
conf.set_quoted('SS_LIVE', livedir + '/')
conf.set('SS_MAX_SERVICE', max_service)
conf.set('SS_MAX_PATH', max_path_size)
conf.set('SS_MAX_SERVICE_NAME', max_service_size)
conf.set('SS_MAX_TREENAME', max_tree_name_size)
conf.set_quoted('SS_DEFAULT_TREENAME', default_tree_name)
conf.set_quoted('SS_SYSTEM_DIR', system_dir + '/')
conf.set_quoted('SS_SKEL_DIR', skeleton_dir + '/')
conf.set_quoted('SS_LOGGER_SYSDIR', system_log_dir + '/')
conf.set_quoted('SS_LOGGER_RUNNER', log_user)
conf.set('SS_LOGGER_TIMESTAMP', log_timestamp)
conf.set('SS_LOGGER_NOTIFY', log_notification)
conf.set_quoted('SS_SERVICE_SYSDIR', system_service_dir + '/')
conf.set_quoted('SS_SERVICE_SYSDIR_USER', system_service_dir + '/user/')
conf.set_quoted('SS_SERVICE_ADMDIR', sysadmin_service_dir + '/')
conf.set_quoted('SS_SERVICE_ADMDIR_USER', sysadmin_service_dir + '/user/')
conf.set_quoted('SS_SERVICE_ADMCONFDIR', sysadmin_service_conf_dir + '/')
conf.set_quoted('SS_SCRIPT_SYSDIR', system_script_dir + '/')
conf.set_quoted('SS_SEED_SYSDIR', system_seed_dir + '/')
conf.set_quoted('SS_SEED_ADMDIR', sysadmin_seed_dir + '/')
conf.set_quoted('SS_ENVIRONMENT_ADMDIR', sysadmin_environment_dir + '/')
conf.set_quoted('SS_USER_DIR', user_dir + '/')
conf.set_quoted('SS_LOGGER_USERDIR', user_log_dir + '/')
conf.set_quoted('SS_SERVICE_USERDIR', user_service_dir + '/')
conf.set_quoted('SS_SERVICE_USERCONFDIR', user_service_conf_dir + '/')
conf.set_quoted('SS_SCRIPT_USERDIR', user_script_dir + '/')
conf.set_quoted('SS_SEED_USERDIR', user_seed_dir + '/')
conf.set_quoted('SS_ENVIRONMENT_USERDIR', user_environment_dir + '/')
conf.set_quoted('SS_BINPREFIX', BINDIR + '/')
conf.set_quoted('SS_EXTBINPREFIX', '')
conf.set_quoted('SS_EXTLIBEXECPREFIX', LIBEXECDIR + '/')
conf.set_quoted('SS_LIBEXECPREFIX', LIBEXECDIR + '/')
conf.set_quoted('SS_EXECLINE_SHEBANGPREFIX', shebangdir + '/')
conf.set_quoted('SS_EXECLINE_BINPREFIX', execline_bindir + '/')
conf.set_quoted('SS_EXECLINE_EXTBINPREFIX', execline_extbindir + '/')

configure_file(
  input: 'config.h.in',
  output: 'config.h',
  configuration: conf,
  install: true,
  install_dir: INCLUDEDIR
)