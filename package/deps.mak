#
# This file has been generated by tools/gen-deps.sh
#

src/include/66/66.h: src/include/66/backup.h src/include/66/config.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/graph.h src/include/66/parser.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/include/66/backup.h: src/include/66/ssexec.h
src/include/66/constants.h: src/include/66/config.h
src/include/66/db.h: src/include/66/graph.h src/include/66/ssexec.h
src/include/66/parser.h: src/include/66/enum.h src/include/66/ssexec.h
src/include/66/rc.h: src/include/66/ssexec.h
src/include/66/resolve.h: src/include/66/parser.h src/include/66/ssexec.h
src/include/66/svc.h: src/include/66/resolve.h src/include/66/ssexec.h
src/include/66/tree.h: src/include/66/ssexec.h
src/include/66/utils.h: src/include/66/resolve.h src/include/66/ssexec.h
src/66/66-all.o src/66/66-all.lo: src/66/66-all.c src/include/66/config.h src/include/66/constants.h src/include/66/tree.h src/include/66/utils.h
src/66/66-dbctl.o src/66/66-dbctl.lo: src/66/66-dbctl.c src/include/66/ssexec.h
src/66/66-disable.o src/66/66-disable.lo: src/66/66-disable.c src/include/66/ssexec.h
src/66/66-enable.o src/66/66-enable.lo: src/66/66-enable.c src/include/66/ssexec.h
src/66/66-info.o src/66/66-info.lo: src/66/66-info.c src/include/66/constants.h src/include/66/enum.h src/include/66/graph.h src/include/66/resolve.h src/include/66/tree.h src/include/66/utils.h
src/66/66-init.o src/66/66-init.lo: src/66/66-init.c src/include/66/ssexec.h
src/66/66-scandir.o src/66/66-scandir.lo: src/66/66-scandir.c src/include/66/config.h src/include/66/constants.h src/include/66/utils.h
src/66/66-start.o src/66/66-start.lo: src/66/66-start.c src/include/66/ssexec.h
src/66/66-stop.o src/66/66-stop.lo: src/66/66-stop.c src/include/66/ssexec.h
src/66/66-svctl.o src/66/66-svctl.lo: src/66/66-svctl.c src/include/66/ssexec.h
src/66/66-tree.o src/66/66-tree.lo: src/66/66-tree.c src/include/66/config.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/tree.h src/include/66/utils.h
src/extra-tools/66-envfile.o src/extra-tools/66-envfile.lo: src/extra-tools/66-envfile.c src/include/66/parser.h
src/extra-tools/execl-cmdline.o src/extra-tools/execl-cmdline.lo: src/extra-tools/execl-cmdline.c
src/lib66/backup_cmd_switcher.o src/lib66/backup_cmd_switcher.lo: src/lib66/backup_cmd_switcher.c src/include/66/constants.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/backup_make_new.o src/lib66/backup_make_new.lo: src/lib66/backup_make_new.c src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/tree.h src/include/66/utils.h
src/lib66/backup_realpath_sym.o src/lib66/backup_realpath_sym.lo: src/lib66/backup_realpath_sym.c src/include/66/constants.h src/include/66/enum.h src/include/66/utils.h
src/lib66/db_cmd_master.o src/lib66/db_cmd_master.lo: src/lib66/db_cmd_master.c src/include/66/constants.h src/include/66/db.h src/include/66/graph.h src/include/66/parser.h src/include/66/utils.h
src/lib66/db_compile.o src/lib66/db_compile.lo: src/lib66/db_compile.c src/include/66/constants.h src/include/66/db.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/db_find_compiled_state.o src/lib66/db_find_compiled_state.lo: src/lib66/db_find_compiled_state.c src/include/66/constants.h src/include/66/utils.h
src/lib66/db_get_permissions.o src/lib66/db_get_permissions.lo: src/lib66/db_get_permissions.c src/include/66/constants.h src/include/66/utils.h
src/lib66/db_ok.o src/lib66/db_ok.lo: src/lib66/db_ok.c src/include/66/constants.h
src/lib66/db_switch_to.o src/lib66/db_switch_to.lo: src/lib66/db_switch_to.c src/include/66/backup.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/db_update.o src/lib66/db_update.lo: src/lib66/db_update.c src/include/66/constants.h src/include/66/db.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/dir_cmpndel.o src/lib66/dir_cmpndel.lo: src/lib66/dir_cmpndel.c src/include/66/utils.h
src/lib66/get_enum.o src/lib66/get_enum.lo: src/lib66/get_enum.c src/include/66/enum.h
src/lib66/get_uidgid.o src/lib66/get_uidgid.lo: src/lib66/get_uidgid.c
src/lib66/get_userhome.o src/lib66/get_userhome.lo: src/lib66/get_userhome.c src/include/66/utils.h
src/lib66/graph.o src/lib66/graph.lo: src/lib66/graph.c src/include/66/constants.h src/include/66/enum.h src/include/66/graph.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/parser.o src/lib66/parser.lo: src/lib66/parser.c src/include/66/config.h src/include/66/constants.h src/include/66/enum.h src/include/66/parser.h src/include/66/utils.h
src/lib66/parser_utils.o src/lib66/parser_utils.lo: src/lib66/parser_utils.c src/include/66/config.h src/include/66/constants.h src/include/66/enum.h src/include/66/parser.h src/include/66/utils.h
src/lib66/parser_write.o src/lib66/parser_write.lo: src/lib66/parser_write.c src/include/66/constants.h src/include/66/enum.h src/include/66/parser.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/rc_init.o src/lib66/rc_init.lo: src/lib66/rc_init.c src/include/66/constants.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/rc_send.o src/lib66/rc_send.lo: src/lib66/rc_send.c src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h
src/lib66/resolve.o src/lib66/resolve.lo: src/lib66/resolve.c src/include/66/constants.h src/include/66/enum.h src/include/66/graph.h src/include/66/parser.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/scandir_ok.o src/lib66/scandir_ok.lo: src/lib66/scandir_ok.c src/include/66/utils.h
src/lib66/scandir_send_signal.o src/lib66/scandir_send_signal.lo: src/lib66/scandir_send_signal.c src/include/66/utils.h
src/lib66/set_info.o src/lib66/set_info.lo: src/lib66/set_info.c src/include/66/config.h src/include/66/utils.h
src/lib66/set_livedir.o src/lib66/set_livedir.lo: src/lib66/set_livedir.c src/include/66/config.h src/include/66/utils.h
src/lib66/set_livescan.o src/lib66/set_livescan.lo: src/lib66/set_livescan.c src/include/66/config.h src/include/66/utils.h
src/lib66/set_livetree.o src/lib66/set_livetree.lo: src/lib66/set_livetree.c src/include/66/config.h src/include/66/utils.h
src/lib66/set_ownerhome.o src/lib66/set_ownerhome.lo: src/lib66/set_ownerhome.c src/include/66/config.h src/include/66/utils.h
src/lib66/set_ownersysdir.o src/lib66/set_ownersysdir.lo: src/lib66/set_ownersysdir.c src/include/66/config.h src/include/66/utils.h
src/lib66/ssexec_dbctl.o src/lib66/ssexec_dbctl.lo: src/lib66/ssexec_dbctl.c src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/tree.h src/include/66/utils.h
src/lib66/ssexec_disable.o src/lib66/ssexec_disable.lo: src/lib66/ssexec_disable.c src/include/66/backup.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/graph.h src/include/66/resolve.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/ssexec_enable.o src/lib66/ssexec_enable.lo: src/lib66/ssexec_enable.c src/include/66/backup.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/graph.h src/include/66/parser.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/ssexec_free.o src/lib66/ssexec_free.lo: src/lib66/ssexec_free.c src/include/66/ssexec.h
src/lib66/ssexec_help.o src/lib66/ssexec_help.lo: src/lib66/ssexec_help.c src/include/66/ssexec.h
src/lib66/ssexec_init.o src/lib66/ssexec_init.lo: src/lib66/ssexec_init.c src/include/66/backup.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/ssexec_main.o src/lib66/ssexec_main.lo: src/lib66/ssexec_main.c src/include/66/ssexec.h src/include/66/tree.h src/include/66/utils.h
src/lib66/ssexec_start.o src/lib66/ssexec_start.lo: src/lib66/ssexec_start.c src/include/66/backup.h src/include/66/config.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/ssexec_stop.o src/lib66/ssexec_stop.lo: src/lib66/ssexec_stop.c src/include/66/backup.h src/include/66/config.h src/include/66/constants.h src/include/66/db.h src/include/66/enum.h src/include/66/rc.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/ssexec_svctl.o src/lib66/ssexec_svctl.lo: src/lib66/ssexec_svctl.c src/include/66/constants.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h src/include/66/tree.h src/include/66/utils.h
src/lib66/sv_alltype_zero.o src/lib66/sv_alltype_zero.lo: src/lib66/sv_alltype_zero.c src/include/66/parser.h src/include/66/resolve.h
src/lib66/svc_init.o src/lib66/svc_init.lo: src/lib66/svc_init.c src/include/66/constants.h src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h src/include/66/utils.h
src/lib66/svc_init_pipe.o src/lib66/svc_init_pipe.lo: src/lib66/svc_init_pipe.c src/include/66/resolve.h src/include/66/svc.h src/include/66/utils.h
src/lib66/svc_send.o src/lib66/svc_send.lo: src/lib66/svc_send.c src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h
src/lib66/svc_switch_to.o src/lib66/svc_switch_to.lo: src/lib66/svc_switch_to.c src/include/66/backup.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/utils.h
src/lib66/svc_unsupervise.o src/lib66/svc_unsupervise.lo: src/lib66/svc_unsupervise.c src/include/66/resolve.h src/include/66/ssexec.h src/include/66/svc.h src/include/66/utils.h
src/lib66/tree_cmd_state.o src/lib66/tree_cmd_state.lo: src/lib66/tree_cmd_state.c src/include/66/constants.h src/include/66/utils.h
src/lib66/tree_copy.o src/lib66/tree_copy.lo: src/lib66/tree_copy.c src/include/66/constants.h src/include/66/tree.h src/include/66/utils.h
src/lib66/tree_copy_tmp.o src/lib66/tree_copy_tmp.lo: src/lib66/tree_copy_tmp.c src/include/66/constants.h src/include/66/enum.h src/include/66/ssexec.h src/include/66/tree.h src/include/66/utils.h
src/lib66/tree_find_current.o src/lib66/tree_find_current.lo: src/lib66/tree_find_current.c src/include/66/constants.h src/include/66/utils.h
src/lib66/tree_get_permissions.o src/lib66/tree_get_permissions.lo: src/lib66/tree_get_permissions.c src/include/66/constants.h src/include/66/utils.h
src/lib66/tree_sethome.o src/lib66/tree_sethome.lo: src/lib66/tree_sethome.c src/include/66/constants.h src/include/66/tree.h
src/lib66/tree_setname.o src/lib66/tree_setname.lo: src/lib66/tree_setname.c src/include/66/tree.h
src/lib66/tree_switch_current.o src/lib66/tree_switch_current.lo: src/lib66/tree_switch_current.c src/include/66/config.h src/include/66/constants.h src/include/66/tree.h src/include/66/utils.h

66-all: EXTRA_LIBS :=
66-all: src/66/66-all.o ${LIB66} -loblibs -ls6 -lskarnet 
66-dbctl: EXTRA_LIBS :=
66-dbctl: src/66/66-dbctl.o ${LIB66} -loblibs -ls6 -lskarnet -ls6rc 
66-disable: EXTRA_LIBS :=
66-disable: src/66/66-disable.o ${LIB66} -loblibs -lexecline -ls6 -lskarnet 
66-enable: EXTRA_LIBS :=
66-enable: src/66/66-enable.o ${LIB66} -loblibs -lexecline -ls6 -lskarnet 
66-info: EXTRA_LIBS :=
66-info: src/66/66-info.o ${LIB66} -loblibs -ls6 -lskarnet
66-init: EXTRA_LIBS :=
66-init: src/66/66-init.o ${LIB66} -loblibs -ls6 -lskarnet
66-scandir: EXTRA_LIBS :=
66-scandir: src/66/66-scandir.o ${LIB66} -loblibs -ls6 -lskarnet  
66-start: EXTRA_LIBS :=
66-start: src/66/66-start.o ${LIB66} -loblibs -ls6 -lskarnet 
66-stop: EXTRA_LIBS :=
66-stop: src/66/66-stop.o ${LIB66} -loblibs -ls6 -lskarnet
66-svctl: EXTRA_LIBS :=
66-svctl: src/66/66-svctl.o ${LIB66} -loblibs -ls6 -lskarnet
66-tree: EXTRA_LIBS :=
66-tree: src/66/66-tree.o ${LIB66} -loblibs -ls6rc -ls6 -lskarnet 
66-envfile: EXTRA_LIBS :=
66-envfile: src/extra-tools/66-envfile.o ${LIB66} -lexecline -loblibs -lskarnet ${LIBEXECLINE}
execl-cmdline: EXTRA_LIBS :=
execl-cmdline: src/extra-tools/execl-cmdline.o -lexecline -loblibs -lskarnet
ifeq ($(strip $(STATIC_LIBS_ARE_PIC)),)
lib66.a.xyzzy: src/lib66/backup_cmd_switcher.o src/lib66/backup_make_new.o src/lib66/backup_realpath_sym.o src/lib66/db_cmd_master.o src/lib66/db_compile.o src/lib66/db_find_compiled_state.o src/lib66/db_get_permissions.o src/lib66/db_ok.o src/lib66/db_switch_to.o src/lib66/db_update.o src/lib66/dir_cmpndel.o src/lib66/get_enum.o src/lib66/get_uidgid.o src/lib66/get_userhome.o src/lib66/graph.o src/lib66/parser.o src/lib66/parser_utils.o src/lib66/parser_write.o src/lib66/rc_init.o src/lib66/rc_send.o src/lib66/resolve.o src/lib66/scandir_ok.o src/lib66/scandir_send_signal.o src/lib66/set_livedir.o src/lib66/set_livescan.o src/lib66/set_livetree.o src/lib66/set_ownerhome.o src/lib66/set_ownersysdir.o src/lib66/ssexec_dbctl.o src/lib66/ssexec_enable.o src/lib66/ssexec_disable.o src/lib66/ssexec_free.o src/lib66/ssexec_help.o src/lib66/ssexec_init.o src/lib66/ssexec_main.o src/lib66/ssexec_start.o src/lib66/ssexec_stop.o src/lib66/ssexec_svctl.o src/lib66/sv_alltype_zero.o src/lib66/svc_init.o src/lib66/svc_init_pipe.o src/lib66/svc_send.o src/lib66/svc_switch_to.o src/lib66/svc_unsupervise.o src/lib66/tree_cmd_state.o src/lib66/tree_copy.o src/lib66/tree_copy_tmp.o src/lib66/tree_find_current.o src/lib66/tree_get_permissions.o src/lib66/tree_sethome.o src/lib66/tree_setname.o src/lib66/tree_switch_current.o
else
lib66.a.xyzzy: src/lib66/backup_cmd_switcher.lo src/lib66/backup_make_new.lo src/lib66/backup_realpath_sym.lo src/lib66/db_cmd_master.lo src/lib66/db_compile.lo src/lib66/db_find_compiled_state.lo src/lib66/db_get_permissions.lo src/lib66/db_ok.lo src/lib66/db_switch_to.lo src/lib66/db_update.lo src/lib66/dir_cmpndel.lo src/lib66/get_enum.lo src/lib66/get_uidgid.lo src/lib66/get_userhome.lo src/lib66/graph.lo src/lib66/parser.lo src/lib66/parser_utils.lo src/lib66/parser_write.lo src/lib66/rc_init.lo src/lib66/rc_send.lo src/lib66/resolve.lo src/lib66/scandir_ok.lo src/lib66/scandir_send_signal.lo src/lib66/set_livedir.lo src/lib66/set_livescan.lo src/lib66/set_livetree.lo src/lib66/set_ownerhome.lo src/lib66/set_ownersysdir.lo src/lib66/ssexec_dbctl.lo src/lib66/ssexec_enable.lo src/lib66/ssexec_disable.lo src/lib66/ssexec_free.lo src/lib66/ssexec_help.lo src/lib66/ssexec_init.lo src/lib66/ssexec_main.lo src/lib66/ssexec_start.lo src/lib66/ssexec_stop.lo src/lib66/ssexec_svctl.lo src/lib66/sv_alltype_zero.lo src/lib66/svc_init.lo src/lib66/svc_init_pipe.lo src/lib66/svc_send.lo src/lib66/svc_switch_to.lo src/lib66/svc_unsupervise.lo src/lib66/tree_cmd_state.lo src/lib66/tree_copy.lo src/lib66/tree_copy_tmp.lo src/lib66/tree_find_current.lo src/lib66/tree_get_permissions.lo src/lib66/tree_sethome.lo src/lib66/tree_setname.lo src/lib66/tree_switch_current.lo
endif
lib66.so.xyzzy: EXTRA_LIBS := -loblibs -lskarnet
lib66.so.xyzzy: src/lib66/backup_cmd_switcher.lo src/lib66/backup_make_new.lo src/lib66/backup_realpath_sym.lo src/lib66/db_cmd_master.lo src/lib66/db_compile.lo src/lib66/db_find_compiled_state.lo src/lib66/db_get_permissions.lo src/lib66/db_ok.lo src/lib66/db_switch_to.lo src/lib66/db_update.lo src/lib66/dir_cmpndel.lo src/lib66/get_enum.lo src/lib66/get_uidgid.lo src/lib66/get_userhome.lo src/lib66/graph.lo src/lib66/parser.lo src/lib66/parser_utils.lo src/lib66/parser_write.lo src/lib66/rc_init.lo src/lib66/rc_send.lo src/lib66/resolve.lo src/lib66/scandir_ok.lo src/lib66/scandir_send_signal.lo src/lib66/set_livedir.lo src/lib66/set_livescan.lo src/lib66/set_livetree.lo src/lib66/set_ownerhome.lo src/lib66/set_ownersysdir.lo src/lib66/ssexec_dbctl.lo src/lib66/ssexec_enable.lo src/lib66/ssexec_disable.lo src/lib66/ssexec_free.lo src/lib66/ssexec_help.lo src/lib66/ssexec_init.lo src/lib66/ssexec_main.lo src/lib66/ssexec_start.lo src/lib66/ssexec_stop.lo src/lib66/ssexec_svctl.lo src/lib66/sv_alltype_zero.lo src/lib66/svc_init.lo src/lib66/svc_init_pipe.lo src/lib66/svc_send.lo src/lib66/svc_switch_to.lo src/lib66/svc_unsupervise.lo src/lib66/tree_cmd_state.lo src/lib66/tree_copy.lo src/lib66/tree_copy_tmp.lo src/lib66/tree_find_current.lo src/lib66/tree_get_permissions.lo src/lib66/tree_sethome.lo src/lib66/tree_setname.lo src/lib66/tree_switch_current.lo
