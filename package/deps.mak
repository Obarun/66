#
# This file has been generated by tools/gen-deps.sh
#

src/include/66/66.h: src/include/66/backup.h src/include/66/config.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/environ.h src/include/66/hpr.h src/include/66/parser.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/include/66/backup.h: src/include/66/ssexec.h
src/include/66/constants.h: src/include/66/config.h
src/include/66/db.h: src/include/66/ssexec.h
src/include/66/hpr.h: src/include/66/constants.h
src/include/66/info.h: src/include/66/enum.h src/include/66/resolve.h
src/include/66/parser.h: src/include/66/enum.h src/include/66/ssexec.h
src/include/66/rc.h: src/include/66/ssexec.h
src/include/66/resolve.h: src/include/66/parser.h src/include/66/ssexec.h
src/include/66/svc.h: src/include/66/resolve.h src/include/66/ssexec.h
src/include/66/tree.h: src/include/66/ssexec.h
src/include/66/utils.h: src/include/66/resolve.h src/include/66/ssexec.h
src/66/66-all.o src/66/66-all.lo: src/66/66-all.c src/include/66/config.h src/include/66/constants.h src/include/66/tree.h src/include/66/utils.h
src/66/66-boot.o src/66/66-boot.lo: src/66/66-boot.c src/include/66/config.h src/include/66/constants.h
src/66/66-dbctl.o src/66/66-dbctl.lo: src/66/66-dbctl.c src/include/66/ssexec.h
src/66/66-disable.o src/66/66-disable.lo: src/66/66-disable.c src/include/66/ssexec.h
src/66/66-enable.o src/66/66-enable.lo: src/66/66-enable.c src/include/66/ssexec.h
src/66/66-env.o src/66/66-env.lo: src/66/66-env.c src/include/66/ssexec.h
src/66/66-hpr.o src/66/66-hpr.lo: src/66/66-hpr.c src/include/66/config.h src/include/66/hpr.h
src/66/66-init.o src/66/66-init.lo: src/66/66-init.c src/include/66/ssexec.h
src/66/66-inresolve.o src/66/66-inresolve.lo: src/66/66-inresolve.c src/include/66/constants.h src/include/66/info.h src/include/66/resolve.h src/include/66/utils.h
src/66/66-inservice.o src/66/66-inservice.lo: src/66/66-inservice.c src/include/66/constants.h src/include/66/enum.h src/include/66/environ.h src/include/66/info.h src/include/66/resolve.h src/include/66/state.h src/include/66/tree.h src/include/66/utils.h
src/66/66-instate.o src/66/66-instate.lo: src/66/66-instate.c src/include/66/constants.h src/include/66/info.h src/include/66/resolve.h src/include/66/state.h src/include/66/utils.h
src/66/66-intree.o src/66/66-intree.lo: src/66/66-intree.c src/include/66/backup.h src/include/66/constants.h src/include/66/enum.h src/include/66/info.h src/include/66/resolve.h src/include/66/tree.h src/include/66/utils.h
src/66/66-parser.o src/66/66-parser.lo: src/66/66-parser.c src/include/66/constants.h src/include/66/parser.h src/include/66/utils.h
src/66/66-scanctl.o src/66/66-scanctl.lo: src/66/66-scanctl.c src/include/66/utils.h
src/66/66-scandir.o src/66/66-scandir.lo: src/66/66-scandir.c src/include/66/config.h src/include/66/constants.h src/include/66/environ.h src/include/66/utils.h
src/66/66-shutdown.o src/66/66-shutdown.lo: src/66/66-shutdown.c src/include/66/config.h src/include/66/hpr.h
src/66/66-shutdownd.o src/66/66-shutdownd.lo: src/66/66-shutdownd.c src/include/66/config.h src/include/66/constants.h
src/66/66-start.o src/66/66-start.lo: src/66/66-start.c src/include/66/ssexec.h
src/66/66-stop.o src/66/66-stop.lo: src/66/66-stop.c src/include/66/ssexec.h
src/66/66-svctl.o src/66/66-svctl.lo: src/66/66-svctl.c src/include/66/ssexec.h
src/66/66-tree.o src/66/66-tree.lo: src/66/66-tree.c src/include/66/config.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/resolve.h src/include/66/state.h src/include/66/tree.h src/include/66/utils.h
src/66/66-update.o src/66/66-update.lo: src/66/66-update.c src/include/66/backup.h src/include/66/constants.h src/include/66/db.h src/include/66/parser.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/extra-tools/66-echo.o src/extra-tools/66-echo.lo: src/extra-tools/66-echo.c
src/extra-tools/66-umountall.o src/extra-tools/66-umountall.lo: src/extra-tools/66-umountall.c src/include/66/config.h
src/lib66/backup_cmd_switcher.o src/lib66/backup_cmd_switcher.lo: src/lib66/backup_cmd_switcher.c src/include/66/constants.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/backup_make_new.o src/lib66/backup_make_new.lo: src/lib66/backup_make_new.c src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/tree.h src/include/66/utils.h
src/lib66/backup_realpath_sym.o src/lib66/backup_realpath_sym.lo: src/lib66/backup_realpath_sym.c src/include/66/constants.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/db_compile.o src/lib66/db_compile.lo: src/lib66/db_compile.c src/include/66/constants.h src/include/66/db.h src/include/66/utils.h
src/lib66/db_find_compiled_state.o src/lib66/db_find_compiled_state.lo: src/lib66/db_find_compiled_state.c src/include/66/constants.h src/include/66/utils.h
src/lib66/db_ok.o src/lib66/db_ok.lo: src/lib66/db_ok.c src/include/66/constants.h
src/lib66/db_switch_to.o src/lib66/db_switch_to.lo: src/lib66/db_switch_to.c src/include/66/backup.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/db_update.o src/lib66/db_update.lo: src/lib66/db_update.c src/include/66/db.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/hpr_shutdown.o src/lib66/hpr_shutdown.lo: src/lib66/hpr_shutdown.c src/include/66/hpr.h
src/lib66/hpr_wall.o src/lib66/hpr_wall.lo: src/lib66/hpr_wall.c src/include/66/hpr.h
src/lib66/parser.o src/lib66/parser.lo: src/lib66/parser.c src/include/66/enum.h src/include/66/parser.h src/include/66/utils.h
src/lib66/parser_enabled.o src/lib66/parser_enabled.lo: src/lib66/parser_enabled.c src/include/66/constants.h src/include/66/environ.h src/include/66/parser.h src/include/66/resolve.h src/include/66/utils.h
src/lib66/parser_module.o src/lib66/parser_module.lo: src/lib66/parser_module.c src/include/66/constants.h src/include/66/environ.h src/include/66/parser.h src/include/66/resolve.h src/include/66/utils.h
src/lib66/parser_utils.o src/lib66/parser_utils.lo: src/lib66/parser_utils.c src/include/66/config.h src/include/66/constants.h src/include/66/enum.h src/include/66/environ.h src/include/66/parser.h src/include/66/utils.h
src/lib66/parser_write.o src/lib66/parser_write.lo: src/lib66/parser_write.c src/include/66/constants.h src/include/66/enum.h src/include/66/environ.h src/include/66/parser.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/rc_init.o src/lib66/rc_init.lo: src/lib66/rc_init.c src/include/66/constants.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/utils.h
src/lib66/rc_manage.o src/lib66/rc_manage.lo: src/lib66/rc_manage.c src/include/66/constants.h src/include/66/db.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/utils.h
src/lib66/rc_send.o src/lib66/rc_send.lo: src/lib66/rc_send.c src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h
src/lib66/rc_unsupervise.o src/lib66/rc_unsupervise.lo: src/lib66/rc_unsupervise.c src/include/66/constants.h src/include/66/db.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/utils.h
src/lib66/ss_environ.o src/lib66/ss_environ.lo: src/lib66/ss_environ.c src/include/66/constants.h src/include/66/environ.h src/include/66/utils.h
src/lib66/ss_get_enum.o src/lib66/ss_get_enum.lo: src/lib66/ss_get_enum.c src/include/66/enum.h
src/lib66/ss_info_utils.o src/lib66/ss_info_utils.lo: src/lib66/ss_info_utils.c src/include/66/info.h src/include/66/resolve.h src/include/66/state.h
src/lib66/ss_instance.o src/lib66/ss_instance.lo: src/lib66/ss_instance.c src/include/66/enum.h src/include/66/utils.h
src/lib66/ss_resolve.o src/lib66/ss_resolve.lo: src/lib66/ss_resolve.c src/include/66/constants.h src/include/66/enum.h src/include/66/parser.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/utils.h
src/lib66/ss_resolve_graph.o src/lib66/ss_resolve_graph.lo: src/lib66/ss_resolve_graph.c src/include/66/constants.h src/include/66/resolve.h src/include/66/utils.h
src/lib66/ss_state.o src/lib66/ss_state.lo: src/lib66/ss_state.c src/include/66/state.h
src/lib66/ss_utils.o src/lib66/ss_utils.lo: src/lib66/ss_utils.c src/include/66/config.h src/include/66/constants.h src/include/66/parser.h src/include/66/resolve.h src/include/66/utils.h
src/lib66/ssexec_dbctl.o src/lib66/ssexec_dbctl.lo: src/lib66/ssexec_dbctl.c src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/utils.h
src/lib66/ssexec_disable.o src/lib66/ssexec_disable.lo: src/lib66/ssexec_disable.c src/include/66/constants.h src/include/66/db.h src/include/66/resolve.h src/include/66/state.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/ssexec_enable.o src/lib66/ssexec_enable.lo: src/lib66/ssexec_enable.c src/include/66/constants.h src/include/66/db.h src/include/66/parser.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/ssexec_env.o src/lib66/ssexec_env.lo: src/lib66/ssexec_env.c src/include/66/config.h src/include/66/environ.h src/include/66/parser.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/ssexec_free.o src/lib66/ssexec_free.lo: src/lib66/ssexec_free.c src/include/66/ssexec.h
src/lib66/ssexec_help.o src/lib66/ssexec_help.lo: src/lib66/ssexec_help.c src/include/66/ssexec.h
src/lib66/ssexec_init.o src/lib66/ssexec_init.lo: src/lib66/ssexec_init.c src/include/66/constants.h src/include/66/db.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/ssexec_main.o src/lib66/ssexec_main.lo: src/lib66/ssexec_main.c src/include/66/ssexec.h src/include/66/tree.h src/include/66/utils.h
src/lib66/ssexec_start.o src/lib66/ssexec_start.lo: src/lib66/ssexec_start.c src/include/66/constants.h src/include/66/db.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/utils.h
src/lib66/ssexec_stop.o src/lib66/ssexec_stop.lo: src/lib66/ssexec_stop.c src/include/66/constants.h src/include/66/db.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/utils.h
src/lib66/ssexec_svctl.o src/lib66/ssexec_svctl.lo: src/lib66/ssexec_svctl.c src/include/66/constants.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/utils.h
src/lib66/svc_init.o src/lib66/svc_init.lo: src/lib66/svc_init.c src/include/66/constants.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/utils.h
src/lib66/svc_init_pipe.o src/lib66/svc_init_pipe.lo: src/lib66/svc_init_pipe.c src/include/66/resolve.h src/include/66/svc.h src/include/66/utils.h
src/lib66/svc_send.o src/lib66/svc_send.lo: src/lib66/svc_send.c src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h
src/lib66/svc_switch_to.o src/lib66/svc_switch_to.lo: src/lib66/svc_switch_to.c src/include/66/backup.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/svc_unsupervise.o src/lib66/svc_unsupervise.lo: src/lib66/svc_unsupervise.c src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/utils.h
src/lib66/tree_cmd_state.o src/lib66/tree_cmd_state.lo: src/lib66/tree_cmd_state.c src/include/66/constants.h src/include/66/utils.h
src/lib66/tree_copy.o src/lib66/tree_copy.lo: src/lib66/tree_copy.c src/include/66/constants.h src/include/66/tree.h
src/lib66/tree_copy_tmp.o src/lib66/tree_copy_tmp.lo: src/lib66/tree_copy_tmp.c src/include/66/constants.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/tree.h src/include/66/utils.h
src/lib66/tree_find_current.o src/lib66/tree_find_current.lo: src/lib66/tree_find_current.c src/include/66/constants.h src/include/66/utils.h
src/lib66/tree_get_permissions.o src/lib66/tree_get_permissions.lo: src/lib66/tree_get_permissions.c src/include/66/constants.h src/include/66/utils.h
src/lib66/tree_sethome.o src/lib66/tree_sethome.lo: src/lib66/tree_sethome.c src/include/66/constants.h src/include/66/tree.h
src/lib66/tree_setname.o src/lib66/tree_setname.lo: src/lib66/tree_setname.c src/include/66/tree.h
src/lib66/tree_switch_current.o src/lib66/tree_switch_current.lo: src/lib66/tree_switch_current.c src/include/66/config.h src/include/66/constants.h src/include/66/tree.h src/include/66/utils.h

66-all: EXTRA_LIBS := ${SPAWN_LIB}
66-all: src/66/66-all.o ${LIB66} -ls6 -loblibs -lskarnet
66-boot: EXTRA_LIBS := ${SPAWN_LIB}
66-boot: src/66/66-boot.o ${LIB66} -ls6 -loblibs -lskarnet
66-dbctl: EXTRA_LIBS := ${SYSCLOCK_LIB} ${SPAWN_LIB}
66-dbctl: src/66/66-dbctl.o ${LIB66} -ls6rc -ls6 -loblibs -lskarnet
66-disable: EXTRA_LIBS :=
66-disable: src/66/66-disable.o ${LIB66} -ls6rc -ls6 -loblibs -lskarnet 
66-enable: EXTRA_LIBS :=
66-enable: src/66/66-enable.o ${LIB66} -ls6rc -ls6 -loblibs -lskarnet
66-env: EXTRA_LIBS :=
66-env: src/66/66-env.o ${LIB66} -loblibs -lskarnet  
66-hpr: EXTRA_LIBS := ${SYSCLOCK_LIB} ${SOCKET_LIB}
66-hpr: src/66/66-hpr.o ${LIB66} ${LIBUTMPS} -loblibs -lskarnet 
66-info: EXTRA_LIBS :=
66-info: src/66/66-info.o ${LIB66} -ls6 -loblibs -lskarnet
66-init: EXTRA_LIBS :=
66-init: src/66/66-init.o ${LIB66} -ls6 -loblibs -lskarnet
66-inresolve: EXTRA_LIBS :=
66-inresolve: src/66/66-inresolve.o ${LIB66} -loblibs -lskarnet
66-inservice: EXTRA_LIBS := ${SPAWN_LIB}
66-inservice: src/66/66-inservice.o ${LIB66} -ls6 -loblibs -lskarnet
66-instate: EXTRA_LIBS :=
66-instate: src/66/66-instate.o ${LIB66} -loblibs -lskarnet
66-intree: EXTRA_LIBS :=
66-intree: src/66/66-intree.o ${LIB66} -ls6 -loblibs -lskarnet
66-parser: EXTRA_LIBS :=
66-parser: src/66/66-parser.o ${LIB66} -loblibs -lskarnet
66-scanctl: EXTRA_LIBS :=
66-scanctl: src/66/66-scanctl.o ${LIB66} -ls6 -loblibs -lskarnet
66-scandir: EXTRA_LIBS :=
66-scandir: src/66/66-scandir.o ${LIB66} -ls6 -loblibs -lskarnet
66-shutdown: EXTRA_LIBS := ${SYSCLOCK_LIB} ${SOCKET_LIB}
66-shutdown: src/66/66-shutdown.o ${LIB66} ${LIBUTMPS} -loblibs -lskarnet
66-shutdownd: EXTRA_LIBS := ${SYSCLOCK_LIB} ${SOCKET_LIB}
66-shutdownd: src/66/66-shutdownd.o ${LIB66} -ls6 -loblibs ${LIBUTMPS} -lskarnet
66-start: EXTRA_LIBS :=
66-start: src/66/66-start.o ${LIB66} -ls6rc -ls6 -loblibs -lskarnet
66-stop: EXTRA_LIBS :=
66-stop: src/66/66-stop.o ${LIB66} -ls6rc -ls6 -loblibs -lskarnet
66-svctl: EXTRA_LIBS := ${SYSCLOCK_LIB}
66-svctl: src/66/66-svctl.o ${LIB66} -ls6 -loblibs -lskarnet
66-tree: EXTRA_LIBS := ${SPAWN_LIB}
66-tree: src/66/66-tree.o ${LIB66} -ls6rc -ls6 -loblibs -lskarnet
66-update: EXTRA_LIBS :=
66-update: src/66/66-update.o ${LIB66} -ls6rc -ls6 -loblibs -lskarnet
66-echo: EXTRA_LIBS :=
66-echo: src/extra-tools/66-echo.o -loblibs -lskarnet 
66-umountall: EXTRA_LIBS :=
66-umountall: src/extra-tools/66-umountall.o -loblibs -lskarnet
ifeq ($(strip $(STATIC_LIBS_ARE_PIC)),)
lib66.a.xyzzy: src/lib66/backup_cmd_switcher.o src/lib66/backup_make_new.o src/lib66/backup_realpath_sym.o src/lib66/db_compile.o src/lib66/db_find_compiled_state.o src/lib66/db_ok.o src/lib66/db_switch_to.o src/lib66/db_update.o src/lib66/hpr_shutdown.o src/lib66/hpr_wall.o src/lib66/parser.o src/lib66/parser_enabled.o src/lib66/parser_module.o src/lib66/parser_utils.o src/lib66/parser_write.o src/lib66/rc_init.o src/lib66/rc_manage.o src/lib66/rc_send.o src/lib66/rc_unsupervise.o src/lib66/ss_environ.o src/lib66/ss_get_enum.o src/lib66/ss_info_utils.o src/lib66/ss_instance.o src/lib66/ss_resolve.o src/lib66/ss_resolve_graph.o src/lib66/ss_state.o src/lib66/ss_utils.o src/lib66/ssexec_dbctl.o src/lib66/ssexec_enable.o src/lib66/ssexec_env.o src/lib66/ssexec_disable.o src/lib66/ssexec_free.o src/lib66/ssexec_help.o src/lib66/ssexec_init.o src/lib66/ssexec_main.o src/lib66/ssexec_start.o src/lib66/ssexec_stop.o src/lib66/ssexec_svctl.o src/lib66/svc_init.o src/lib66/svc_init_pipe.o src/lib66/svc_send.o src/lib66/svc_switch_to.o src/lib66/svc_unsupervise.o src/lib66/tree_cmd_state.o src/lib66/tree_copy.o src/lib66/tree_copy_tmp.o src/lib66/tree_find_current.o src/lib66/tree_get_permissions.o src/lib66/tree_sethome.o src/lib66/tree_setname.o src/lib66/tree_switch_current.o
else
lib66.a.xyzzy: src/lib66/backup_cmd_switcher.lo src/lib66/backup_make_new.lo src/lib66/backup_realpath_sym.lo src/lib66/db_compile.lo src/lib66/db_find_compiled_state.lo src/lib66/db_ok.lo src/lib66/db_switch_to.lo src/lib66/db_update.lo src/lib66/hpr_shutdown.lo src/lib66/hpr_wall.lo src/lib66/parser.lo src/lib66/parser_enabled.lo src/lib66/parser_module.lo src/lib66/parser_utils.lo src/lib66/parser_write.lo src/lib66/rc_init.lo src/lib66/rc_manage.lo src/lib66/rc_send.lo src/lib66/rc_unsupervise.lo src/lib66/ss_environ.lo src/lib66/ss_get_enum.lo src/lib66/ss_info_utils.lo src/lib66/ss_instance.lo src/lib66/ss_resolve.lo src/lib66/ss_resolve_graph.lo src/lib66/ss_state.lo src/lib66/ss_utils.lo src/lib66/ssexec_dbctl.lo src/lib66/ssexec_enable.lo src/lib66/ssexec_env.lo src/lib66/ssexec_disable.lo src/lib66/ssexec_free.lo src/lib66/ssexec_help.lo src/lib66/ssexec_init.lo src/lib66/ssexec_main.lo src/lib66/ssexec_start.lo src/lib66/ssexec_stop.lo src/lib66/ssexec_svctl.lo src/lib66/svc_init.lo src/lib66/svc_init_pipe.lo src/lib66/svc_send.lo src/lib66/svc_switch_to.lo src/lib66/svc_unsupervise.lo src/lib66/tree_cmd_state.lo src/lib66/tree_copy.lo src/lib66/tree_copy_tmp.lo src/lib66/tree_find_current.lo src/lib66/tree_get_permissions.lo src/lib66/tree_sethome.lo src/lib66/tree_setname.lo src/lib66/tree_switch_current.lo
endif
lib66.so.xyzzy: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lexecline -lskarnet
lib66.so.xyzzy: src/lib66/backup_cmd_switcher.lo src/lib66/backup_make_new.lo src/lib66/backup_realpath_sym.lo src/lib66/db_compile.lo src/lib66/db_find_compiled_state.lo src/lib66/db_ok.lo src/lib66/db_switch_to.lo src/lib66/db_update.lo src/lib66/hpr_shutdown.lo src/lib66/hpr_wall.lo src/lib66/parser.lo src/lib66/parser_enabled.lo src/lib66/parser_module.lo src/lib66/parser_utils.lo src/lib66/parser_write.lo src/lib66/rc_init.lo src/lib66/rc_manage.lo src/lib66/rc_send.lo src/lib66/rc_unsupervise.lo src/lib66/ss_environ.lo src/lib66/ss_get_enum.lo src/lib66/ss_info_utils.lo src/lib66/ss_instance.lo src/lib66/ss_resolve.lo src/lib66/ss_resolve_graph.lo src/lib66/ss_state.lo src/lib66/ss_utils.lo src/lib66/ssexec_dbctl.lo src/lib66/ssexec_enable.lo src/lib66/ssexec_env.lo src/lib66/ssexec_disable.lo src/lib66/ssexec_free.lo src/lib66/ssexec_help.lo src/lib66/ssexec_init.lo src/lib66/ssexec_main.lo src/lib66/ssexec_start.lo src/lib66/ssexec_stop.lo src/lib66/ssexec_svctl.lo src/lib66/svc_init.lo src/lib66/svc_init_pipe.lo src/lib66/svc_send.lo src/lib66/svc_switch_to.lo src/lib66/svc_unsupervise.lo src/lib66/tree_cmd_state.lo src/lib66/tree_copy.lo src/lib66/tree_copy_tmp.lo src/lib66/tree_find_current.lo src/lib66/tree_get_permissions.lo src/lib66/tree_sethome.lo src/lib66/tree_setname.lo src/lib66/tree_switch_current.lo
