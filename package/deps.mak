#
# This file has been generated by tools/gen-deps.sh
#

src/include/66/66.h: src/include/66/backup.h src/include/66/config.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/environ.h src/include/66/hpr.h src/include/66/info.h src/include/66/parser.h src/include/66/rc.h src/include/66/resolve.h src/include/66/service.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/include/66/backup.h: src/include/66/ssexec.h
src/include/66/constants.h: src/include/66/config.h
src/include/66/db.h: src/include/66/ssexec.h
src/include/66/environ.h: src/include/66/parser.h
src/include/66/hpr.h: src/include/66/constants.h
src/include/66/info.h: src/include/66/enum.h src/include/66/service.h
src/include/66/parser.h: src/include/66/enum.h src/include/66/ssexec.h
src/include/66/rc.h: src/include/66/ssexec.h
src/include/66/resolve.h: src/include/66/parser.h src/include/66/ssexec.h
src/include/66/service.h: src/include/66/parser.h src/include/66/resolve.h src/include/66/ssexec.h
src/include/66/svc.h: src/include/66/service.h src/include/66/ssexec.h
src/include/66/tree.h: src/include/66/resolve.h src/include/66/ssexec.h
src/include/66/utils.h: src/include/66/service.h src/include/66/ssexec.h
src/66/66-all.o src/66/66-all.lo: src/66/66-all.c src/include/66/ssexec.h
src/66/66-boot.o src/66/66-boot.lo: src/66/66-boot.c src/include/66/config.h src/include/66/constants.h
src/66/66-dbctl.o src/66/66-dbctl.lo: src/66/66-dbctl.c src/include/66/ssexec.h
src/66/66-disable.o src/66/66-disable.lo: src/66/66-disable.c src/include/66/ssexec.h
src/66/66-enable.o src/66/66-enable.lo: src/66/66-enable.c src/include/66/ssexec.h
src/66/66-env.o src/66/66-env.lo: src/66/66-env.c src/include/66/ssexec.h
src/66/66-hpr.o src/66/66-hpr.lo: src/66/66-hpr.c src/include/66/config.h src/include/66/hpr.h
src/66/66-init.o src/66/66-init.lo: src/66/66-init.c src/include/66/ssexec.h
src/66/66-inresolve.o src/66/66-inresolve.lo: src/66/66-inresolve.c src/include/66/constants.h src/include/66/info.h src/include/66/resolve.h src/include/66/service.h src/include/66/tree.h src/include/66/utils.h
src/66/66-inservice.o src/66/66-inservice.lo: src/66/66-inservice.c src/include/66/constants.h src/include/66/enum.h src/include/66/environ.h src/include/66/graph.h src/include/66/info.h src/include/66/resolve.h src/include/66/service.h src/include/66/state.h src/include/66/tree.h src/include/66/utils.h
src/66/66-instate.o src/66/66-instate.lo: src/66/66-instate.c src/include/66/constants.h src/include/66/info.h src/include/66/resolve.h src/include/66/state.h src/include/66/utils.h
src/66/66-intree.o src/66/66-intree.lo: src/66/66-intree.c src/include/66/backup.h src/include/66/constants.h src/include/66/enum.h src/include/66/graph.h src/include/66/info.h src/include/66/resolve.h src/include/66/service.h src/include/66/tree.h src/include/66/utils.h
src/66/66-parser.o src/66/66-parser.lo: src/66/66-parser.c src/include/66/constants.h src/include/66/parser.h src/include/66/utils.h
src/66/66-scanctl.o src/66/66-scanctl.lo: src/66/66-scanctl.c src/include/66/utils.h
src/66/66-scandir.o src/66/66-scandir.lo: src/66/66-scandir.c src/include/66/config.h src/include/66/constants.h src/include/66/utils.h
src/66/66-shutdown.o src/66/66-shutdown.lo: src/66/66-shutdown.c src/include/66/config.h src/include/66/hpr.h
src/66/66-shutdownd.o src/66/66-shutdownd.lo: src/66/66-shutdownd.c src/include/66/config.h src/include/66/constants.h
src/66/66-start.o src/66/66-start.lo: src/66/66-start.c src/include/66/ssexec.h
src/66/66-stop.o src/66/66-stop.lo: src/66/66-stop.c src/include/66/ssexec.h
src/66/66-svctl.o src/66/66-svctl.lo: src/66/66-svctl.c src/include/66/ssexec.h
src/66/66-tree.o src/66/66-tree.lo: src/66/66-tree.c src/include/66/ssexec.h
src/extra-tools/66-echo.o src/extra-tools/66-echo.lo: src/extra-tools/66-echo.c
src/extra-tools/66-nuke.o src/extra-tools/66-nuke.lo: src/extra-tools/66-nuke.c
src/extra-tools/66-umountall.o src/extra-tools/66-umountall.lo: src/extra-tools/66-umountall.c src/include/66/config.h
src/extra-tools/execl-envfile.o src/extra-tools/execl-envfile.lo: src/extra-tools/execl-envfile.c
src/lib66/backup/backup_cmd_switcher.o src/lib66/backup/backup_cmd_switcher.lo: src/lib66/backup/backup_cmd_switcher.c src/include/66/constants.h src/include/66/enum.h src/include/66/ssexec.h
src/lib66/backup/backup_make_new.o src/lib66/backup/backup_make_new.lo: src/lib66/backup/backup_make_new.c src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/resolve.h src/include/66/tree.h
src/lib66/backup/backup_realpath_sym.o src/lib66/backup/backup_realpath_sym.lo: src/lib66/backup/backup_realpath_sym.c src/include/66/constants.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/db/db_compile.o src/lib66/db/db_compile.lo: src/lib66/db/db_compile.c src/include/66/constants.h src/include/66/db.h src/include/66/utils.h
src/lib66/db/db_find_compiled_state.o src/lib66/db/db_find_compiled_state.lo: src/lib66/db/db_find_compiled_state.c src/include/66/constants.h src/include/66/utils.h
src/lib66/db/db_ok.o src/lib66/db/db_ok.lo: src/lib66/db/db_ok.c src/include/66/constants.h
src/lib66/db/db_switch_to.o src/lib66/db/db_switch_to.lo: src/lib66/db/db_switch_to.c src/include/66/backup.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/db/db_update.o src/lib66/db/db_update.lo: src/lib66/db/db_update.c src/include/66/db.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/enum/enum.o src/lib66/enum/enum.lo: src/lib66/enum/enum.c src/include/66/enum.h
src/lib66/environ/environ.o src/lib66/environ/environ.lo: src/lib66/environ/environ.c src/include/66/constants.h src/include/66/enum.h src/include/66/environ.h src/include/66/utils.h
src/lib66/exec/ssexec_all.o src/lib66/exec/ssexec_all.lo: src/lib66/exec/ssexec_all.c src/include/66/constants.h src/include/66/db.h src/include/66/graph.h src/include/66/ssexec.h src/include/66/tree.h src/include/66/utils.h
src/lib66/exec/ssexec_dbctl.o src/lib66/exec/ssexec_dbctl.lo: src/lib66/exec/ssexec_dbctl.c src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/resolve.h src/include/66/service.h src/include/66/ssexec.h src/include/66/state.h src/include/66/utils.h
src/lib66/exec/ssexec_disable.o src/lib66/exec/ssexec_disable.lo: src/lib66/exec/ssexec_disable.c src/include/66/constants.h src/include/66/db.h src/include/66/resolve.h src/include/66/service.h src/include/66/state.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/exec/ssexec_enable.o src/lib66/exec/ssexec_enable.lo: src/lib66/exec/ssexec_enable.c src/include/66/constants.h src/include/66/db.h src/include/66/environ.h src/include/66/parser.h src/include/66/resolve.h src/include/66/service.h src/include/66/ssexec.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/exec/ssexec_env.o src/lib66/exec/ssexec_env.lo: src/lib66/exec/ssexec_env.c src/include/66/config.h src/include/66/constants.h src/include/66/environ.h src/include/66/parser.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/exec/ssexec_free.o src/lib66/exec/ssexec_free.lo: src/lib66/exec/ssexec_free.c src/include/66/ssexec.h
src/lib66/exec/ssexec_help.o src/lib66/exec/ssexec_help.lo: src/lib66/exec/ssexec_help.c src/include/66/ssexec.h
src/lib66/exec/ssexec_init.o src/lib66/exec/ssexec_init.lo: src/lib66/exec/ssexec_init.c src/include/66/constants.h src/include/66/db.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/exec/ssexec_main.o src/lib66/exec/ssexec_main.lo: src/lib66/exec/ssexec_main.c src/include/66/constants.h src/include/66/ssexec.h src/include/66/tree.h src/include/66/utils.h
src/lib66/exec/ssexec_start.o src/lib66/exec/ssexec_start.lo: src/lib66/exec/ssexec_start.c src/include/66/constants.h src/include/66/db.h src/include/66/rc.h src/include/66/resolve.h src/include/66/service.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/utils.h
src/lib66/exec/ssexec_stop.o src/lib66/exec/ssexec_stop.lo: src/lib66/exec/ssexec_stop.c src/include/66/constants.h src/include/66/db.h src/include/66/rc.h src/include/66/resolve.h src/include/66/service.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/utils.h
src/lib66/exec/ssexec_svctl.o src/lib66/exec/ssexec_svctl.lo: src/lib66/exec/ssexec_svctl.c src/include/66/constants.h src/include/66/resolve.h src/include/66/service.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/utils.h
src/lib66/exec/ssexec_tree.o src/lib66/exec/ssexec_tree.lo: src/lib66/exec/ssexec_tree.c src/include/66/config.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/graph.h src/include/66/resolve.h src/include/66/service.h src/include/66/state.h src/include/66/tree.h src/include/66/utils.h
src/lib66/graph/graph.o src/lib66/graph/graph.lo: src/lib66/graph/graph.c src/include/66/constants.h src/include/66/enum.h src/include/66/graph.h src/include/66/resolve.h src/include/66/service.h src/include/66/tree.h
src/lib66/graph/ss_resolve_graph.o src/lib66/graph/ss_resolve_graph.lo: src/lib66/graph/ss_resolve_graph.c src/include/66/constants.h src/include/66/resolve.h src/include/66/service.h src/include/66/utils.h
src/lib66/info/info_utils.o src/lib66/info/info_utils.lo: src/lib66/info/info_utils.c src/include/66/constants.h src/include/66/info.h src/include/66/resolve.h src/include/66/state.h src/include/66/tree.h src/include/66/utils.h
src/lib66/instance/instance.o src/lib66/instance/instance.lo: src/lib66/instance/instance.c src/include/66/enum.h src/include/66/utils.h
src/lib66/parse/parse_module.o src/lib66/parse/parse_module.lo: src/lib66/parse/parse_module.c src/include/66/constants.h src/include/66/environ.h src/include/66/parser.h src/include/66/resolve.h src/include/66/utils.h
src/lib66/parse/parse_service.o src/lib66/parse/parse_service.lo: src/lib66/parse/parse_service.c src/include/66/constants.h src/include/66/parser.h src/include/66/service.h src/include/66/ssexec.h src/include/66/tree.h src/include/66/utils.h
src/lib66/parse/parser.o src/lib66/parse/parser.lo: src/lib66/parse/parser.c src/include/66/enum.h src/include/66/parser.h
src/lib66/parse/parser_utils.o src/lib66/parse/parser_utils.lo: src/lib66/parse/parser_utils.c src/include/66/config.h src/include/66/constants.h src/include/66/enum.h src/include/66/environ.h src/include/66/parser.h src/include/66/utils.h
src/lib66/parse/parser_write.o src/lib66/parse/parser_write.lo: src/lib66/parse/parser_write.c src/include/66/constants.h src/include/66/enum.h src/include/66/environ.h src/include/66/parser.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/rc/rc_init.o src/lib66/rc/rc_init.lo: src/lib66/rc/rc_init.c src/include/66/constants.h src/include/66/rc.h src/include/66/resolve.h src/include/66/service.h src/include/66/ssexec.h src/include/66/state.h src/include/66/utils.h
src/lib66/rc/rc_manage.o src/lib66/rc/rc_manage.lo: src/lib66/rc/rc_manage.c src/include/66/constants.h src/include/66/db.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/utils.h
src/lib66/rc/rc_send.o src/lib66/rc/rc_send.lo: src/lib66/rc/rc_send.c src/include/66/rc.h src/include/66/resolve.h src/include/66/service.h src/include/66/ssexec.h
src/lib66/rc/rc_unsupervise.o src/lib66/rc/rc_unsupervise.lo: src/lib66/rc/rc_unsupervise.c src/include/66/constants.h src/include/66/db.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/utils.h
src/lib66/resolve/resolve.o src/lib66/resolve/resolve.lo: src/lib66/resolve/resolve.c src/include/66/constants.h src/include/66/graph.h src/include/66/resolve.h src/include/66/service.h src/include/66/tree.h
src/lib66/service/service.o src/lib66/service/service.lo: src/lib66/service/service.c src/include/66/constants.h src/include/66/enum.h src/include/66/parser.h src/include/66/resolve.h src/include/66/service.h src/include/66/ssexec.h src/include/66/state.h src/include/66/utils.h
src/lib66/shutdown/hpr_shutdown.o src/lib66/shutdown/hpr_shutdown.lo: src/lib66/shutdown/hpr_shutdown.c src/include/66/hpr.h
src/lib66/shutdown/hpr_wall.o src/lib66/shutdown/hpr_wall.lo: src/lib66/shutdown/hpr_wall.c src/include/66/hpr.h
src/lib66/state/state.o src/lib66/state/state.lo: src/lib66/state/state.c src/include/66/state.h
src/lib66/svc/svc_init.o src/lib66/svc/svc_init.lo: src/lib66/svc/svc_init.c src/include/66/constants.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/utils.h
src/lib66/svc/svc_init_pipe.o src/lib66/svc/svc_init_pipe.lo: src/lib66/svc/svc_init_pipe.c src/include/66/resolve.h src/include/66/svc.h src/include/66/utils.h
src/lib66/svc/svc_send.o src/lib66/svc/svc_send.lo: src/lib66/svc/svc_send.c src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h
src/lib66/svc/svc_switch_to.o src/lib66/svc/svc_switch_to.lo: src/lib66/svc/svc_switch_to.c src/include/66/backup.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/svc/svc_unsupervise.o src/lib66/svc/svc_unsupervise.lo: src/lib66/svc/svc_unsupervise.c src/include/66/resolve.h src/include/66/ssexec.h src/include/66/state.h src/include/66/svc.h src/include/66/utils.h
src/lib66/tree/tree.o src/lib66/tree/tree.lo: src/lib66/tree/tree.c src/include/66/constants.h src/include/66/resolve.h src/include/66/tree.h
src/lib66/tree/tree_cmd_state.o src/lib66/tree/tree_cmd_state.lo: src/lib66/tree/tree_cmd_state.c src/include/66/constants.h src/include/66/utils.h
src/lib66/tree/tree_copy.o src/lib66/tree/tree_copy.lo: src/lib66/tree/tree_copy.c src/include/66/constants.h src/include/66/tree.h
src/lib66/tree/tree_copy_tmp.o src/lib66/tree/tree_copy_tmp.lo: src/lib66/tree/tree_copy_tmp.c src/include/66/constants.h src/include/66/enum.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/tree.h src/include/66/utils.h
src/lib66/tree/tree_get_permissions.o src/lib66/tree/tree_get_permissions.lo: src/lib66/tree/tree_get_permissions.c src/include/66/constants.h src/include/66/utils.h
src/lib66/tree/tree_resolve.o src/lib66/tree/tree_resolve.lo: src/lib66/tree/tree_resolve.c src/include/66/constants.h src/include/66/graph.h src/include/66/resolve.h src/include/66/tree.h
src/lib66/tree/tree_seed.o src/lib66/tree/tree_seed.lo: src/lib66/tree/tree_seed.c src/include/66/config.h src/include/66/enum.h src/include/66/tree.h src/include/66/utils.h
src/lib66/tree/tree_sethome.o src/lib66/tree/tree_sethome.lo: src/lib66/tree/tree_sethome.c src/include/66/constants.h src/include/66/ssexec.h src/include/66/tree.h
src/lib66/tree/tree_setname.o src/lib66/tree/tree_setname.lo: src/lib66/tree/tree_setname.c src/include/66/tree.h
src/lib66/tree/tree_switch_current.o src/lib66/tree/tree_switch_current.lo: src/lib66/tree/tree_switch_current.c src/include/66/constants.h src/include/66/resolve.h src/include/66/tree.h
src/lib66/utils/utils.o src/lib66/utils/utils.lo: src/lib66/utils/utils.c src/include/66/config.h src/include/66/constants.h src/include/66/parser.h src/include/66/resolve.h src/include/66/service.h src/include/66/utils.h
66-all: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lskarnet
66-all: src/66/66-all.o ${LIB66}
66-boot: EXTRA_LIBS := -ls6 -loblibs -lskarnet ${SPAWN_LIB}
66-boot: src/66/66-boot.o ${LIB66}
66-dbctl: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lskarnet ${SYSCLOCK_LIB} ${SPAWN_LIB}
66-dbctl: src/66/66-dbctl.o ${LIB66}
66-disable: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lskarnet
66-disable: src/66/66-disable.o ${LIB66} 
66-enable: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lskarnet
66-enable: src/66/66-enable.o ${LIB66}
66-env: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lskarnet
66-env: src/66/66-env.o ${LIB66}  
66-hpr: EXTRA_LIBS := -loblibs -lskarnet ${SYSCLOCK_LIB} ${SOCKET_LIB}
66-hpr: src/66/66-hpr.o ${LIB66} ${LIBUTMPS} 
66-info: EXTRA_LIBS := -ls6 -loblibs -lskarnet
66-info: src/66/66-info.o ${LIB66}
66-init: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lskarnet
66-init: src/66/66-init.o ${LIB66}
66-inresolve: EXTRA_LIBS := -loblibs -lskarnet
66-inresolve: src/66/66-inresolve.o ${LIB66}
66-inservice: EXTRA_LIBS := -ls6 -loblibs -lskarnet ${SPAWN_LIB}
66-inservice: src/66/66-inservice.o ${LIB66}
66-instate: EXTRA_LIBS := -loblibs -lskarnet
66-instate: src/66/66-instate.o ${LIB66}
66-intree: EXTRA_LIBS := -ls6 -loblibs -lskarnet
66-intree: src/66/66-intree.o ${LIB66}
66-parser: EXTRA_LIBS := -loblibs -lskarnet
66-parser: src/66/66-parser.o ${LIB66}
66-scanctl: EXTRA_LIBS := -ls6 -loblibs -lskarnet
66-scanctl: src/66/66-scanctl.o ${LIB66}
66-scandir: EXTRA_LIBS := -ls6 -loblibs -lskarnet
66-scandir: src/66/66-scandir.o ${LIB66}
66-shutdown: EXTRA_LIBS := -loblibs -lskarnet ${SYSCLOCK_LIB} ${SOCKET_LIB}
66-shutdown: src/66/66-shutdown.o ${LIB66} ${LIBUTMPS}
66-shutdownd: EXTRA_LIBS := -ls6 -loblibs -lskarnet ${SYSCLOCK_LIB} ${SOCKET_LIB}
66-shutdownd: src/66/66-shutdownd.o ${LIB66} ${LIBUTMPS}
66-start: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lskarnet
66-start: src/66/66-start.o ${LIB66}
66-stop: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lskarnet
66-stop: src/66/66-stop.o ${LIB66}
66-svctl: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lskarnet ${SYSCLOCK_LIB}
66-svctl: src/66/66-svctl.o ${LIB66}
66-tree: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lskarnet ${SPAWN_LIB}
66-tree: src/66/66-tree.o ${LIB66}
66-echo: EXTRA_LIBS := -loblibs -lskarnet
66-echo: src/extra-tools/66-echo.o 
66-nuke: EXTRA_LIBS := -loblibs -lskarnet
66-nuke: src/extra-tools/66-nuke.o
66-umountall: EXTRA_LIBS := -loblibs -lskarnet
66-umountall: src/extra-tools/66-umountall.o
execl-envfile: EXTRA_LIBS := -loblibs -lexecline -lskarnet
execl-envfile: src/extra-tools/execl-envfile.o ${LIBEXECLINE} 
ifeq ($(strip $(STATIC_LIBS_ARE_PIC)),)
lib66.a.xyzzy: src/lib66/backup/backup_cmd_switcher.o src/lib66/backup/backup_make_new.o src/lib66/backup/backup_realpath_sym.o src/lib66/db/db_compile.o src/lib66/db/db_find_compiled_state.o src/lib66/db/db_ok.o src/lib66/db/db_switch_to.o src/lib66/db/db_update.o src/lib66/enum/enum.o src/lib66/environ/environ.o src/lib66/exec/ssexec_all.o src/lib66/exec/ssexec_dbctl.o src/lib66/exec/ssexec_disable.o src/lib66/exec/ssexec_enable.o src/lib66/exec/ssexec_env.o src/lib66/exec/ssexec_free.o src/lib66/exec/ssexec_help.o src/lib66/exec/ssexec_init.o src/lib66/exec/ssexec_main.o src/lib66/exec/ssexec_start.o src/lib66/exec/ssexec_stop.o src/lib66/exec/ssexec_svctl.o src/lib66/exec/ssexec_tree.o src/lib66/graph/graph.o src/lib66/graph/ss_resolve_graph.o src/lib66/info/info_utils.o src/lib66/instance/instance.o src/lib66/parse/parse_module.o src/lib66/parse/parse_service.o src/lib66/parse/parser.o src/lib66/parse/parser_utils.o src/lib66/parse/parser_write.o src/lib66/rc/rc_init.o src/lib66/rc/rc_manage.o src/lib66/rc/rc_send.o src/lib66/rc/rc_unsupervise.o src/lib66/resolve/resolve.o src/lib66/service/service.o src/lib66/shutdown/hpr_shutdown.o src/lib66/shutdown/hpr_wall.o src/lib66/state/state.o src/lib66/svc/svc_init.o src/lib66/svc/svc_init_pipe.o src/lib66/svc/svc_send.o src/lib66/svc/svc_switch.o src/lib66/svc/svc_unsupervise.o src/lib66/tree/tree.o src/lib66/tree/tree_cmd_state.o src/lib66/tree/tree_copy_tmp.o src/lib66/tree/tree_get_permissions.o src/lib66/tree/tree_resolve.o src/lib66/tree/tree_seed.o src/lib66/tree/tree_sethome.o src/lib66/tree/tree_setname.o src/lib66/tree/tree_switch_current.o src/lib66/utils/utils.o
else
lib66.a.xyzzy: src/lib66/backup/backup_cmd_switcher.lo src/lib66/backup/backup_make_new.lo src/lib66/backup/backup_realpath_sym.lo src/lib66/db/db_compile.lo src/lib66/db/db_find_compiled_state.lo src/lib66/db/db_ok.lo src/lib66/db/db_switch_to.lo src/lib66/db/db_update.lo src/lib66/enum/enum.lo src/lib66/environ/environ.lo src/lib66/exec/ssexec_all.lo src/lib66/exec/ssexec_dbctl.lo src/lib66/exec/ssexec_disable.lo src/lib66/exec/ssexec_enable.lo src/lib66/exec/ssexec_env.lo src/lib66/exec/ssexec_free.lo src/lib66/exec/ssexec_help.lo src/lib66/exec/ssexec_init.lo src/lib66/exec/ssexec_main.lo src/lib66/exec/ssexec_start.lo src/lib66/exec/ssexec_stop.lo src/lib66/exec/ssexec_svctl.lo src/lib66/exec/ssexec_tree.lo src/lib66/graph/graph.lo src/lib66/graph/ss_resolve_graph.lo src/lib66/info/info_utils.lo src/lib66/instance/instance.lo src/lib66/parse/parse_module.lo src/lib66/parse/parse_service.lo src/lib66/parse/parser.lo src/lib66/parse/parser_utils.lo src/lib66/parse/parser_write.lo src/lib66/rc/rc_init.lo src/lib66/rc/rc_manage.lo src/lib66/rc/rc_send.lo src/lib66/rc/rc_unsupervise.lo src/lib66/resolve/resolve.lo src/lib66/service/service.lo src/lib66/shutdown/hpr_shutdown.lo src/lib66/shutdown/hpr_wall.lo src/lib66/state/state.lo src/lib66/svc/svc_init.lo src/lib66/svc/svc_init_pipe.lo src/lib66/svc/svc_send.lo src/lib66/svc/svc_switch.lo src/lib66/svc/svc_unsupervise.lo src/lib66/tree/tree.lo src/lib66/tree/tree_cmd_state.lo src/lib66/tree/tree_copy_tmp.lo src/lib66/tree/tree_get_permissions.lo src/lib66/tree/tree_resolve.lo src/lib66/tree/tree_seed.lo src/lib66/tree/tree_sethome.lo src/lib66/tree/tree_setname.lo src/lib66/tree/tree_switch_current.lo src/lib66/utils/utils.lo
endif
lib66.so.xyzzy: EXTRA_LIBS := -ls6rc -ls6 -loblibs -lexecline -lskarnet
lib66.so.xyzzy: src/lib66/backup/backup_cmd_switcher.lo src/lib66/backup/backup_make_new.lo src/lib66/backup/backup_realpath_sym.lo src/lib66/db/db_compile.lo src/lib66/db/db_find_compiled_state.lo src/lib66/db/db_ok.lo src/lib66/db/db_switch_to.lo src/lib66/db/db_update.lo src/lib66/enum/enum.lo src/lib66/environ/environ.lo src/lib66/exec/ssexec_all.lo src/lib66/exec/ssexec_dbctl.lo src/lib66/exec/ssexec_disable.lo src/lib66/exec/ssexec_enable.lo src/lib66/exec/ssexec_env.lo src/lib66/exec/ssexec_free.lo src/lib66/exec/ssexec_help.lo src/lib66/exec/ssexec_init.lo src/lib66/exec/ssexec_main.lo src/lib66/exec/ssexec_start.lo src/lib66/exec/ssexec_stop.lo src/lib66/exec/ssexec_svctl.lo src/lib66/exec/ssexec_tree.lo src/lib66/graph/graph.lo src/lib66/graph/ss_resolve_graph.lo src/lib66/info/info_utils.lo src/lib66/instance/instance.lo src/lib66/parse/parse_module.lo src/lib66/parse/parse_service.lo src/lib66/parse/parser.lo src/lib66/parse/parser_utils.lo src/lib66/parse/parser_write.lo src/lib66/rc/rc_init.lo src/lib66/rc/rc_manage.lo src/lib66/rc/rc_send.lo src/lib66/rc/rc_unsupervise.lo src/lib66/resolve/resolve.lo src/lib66/service/service.lo src/lib66/shutdown/hpr_shutdown.lo src/lib66/shutdown/hpr_wall.lo src/lib66/state/state.lo src/lib66/svc/svc_init.lo src/lib66/svc/svc_init_pipe.lo src/lib66/svc/svc_send.lo src/lib66/svc/svc_switch.lo src/lib66/svc/svc_unsupervise.lo src/lib66/tree/tree.lo src/lib66/tree/tree_cmd_state.lo src/lib66/tree/tree_copy_tmp.lo src/lib66/tree/tree_get_permissions.lo src/lib66/tree/tree_resolve.lo src/lib66/tree/tree_seed.lo src/lib66/tree/tree_sethome.lo src/lib66/tree/tree_setname.lo src/lib66/tree/tree_switch_current.lo src/lib66/utils/utils.lo
