/*
 * ssexec_snapshot_create.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/stack.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>

#include <skalibs/sgetopt.h>
#include <skalibs/djbunix.h>

#include <66/snapshot.h>
#include <66/ssexec.h>
#include <66/constants.h>
#include <66/utils.h>

snapshot_list_t snapshot_root_list[] = {
    { .name = SS_SKEL_DIR },
    { .name = SS_SERVICE_SYSDIR },
    { .name = SS_SERVICE_ADMDIR },
    { .name = SS_SERVICE_ADMCONFDIR },
    { .name = SS_SCRIPT_SYSDIR },
    { .name = SS_SEED_SYSDIR },
    { .name = SS_SEED_ADMDIR },
    { .name = SS_ENVIRONMENT_ADMDIR },
    { .name = 0 }
} ;

snapshot_list_t snapshot_user_list[] = {
    { .name = SS_SERVICE_USERDIR },
    { .name = SS_SERVICE_USERCONFDIR },
    { .name = SS_SCRIPT_USERDIR },
    { .name = SS_SEED_USERDIR },
    { .name = SS_ENVIRONMENT_USERDIR },
    { .name = 0 }
} ;

static void snapshot_cleanup(const char *dir)
{
    if (!dir_rm_rf(dir))
        log_warnu("remove service directory: ", dir) ;
}

static int copy_dir(const char *src, char *dst, size_t len, ssexec_t *info)
{
    log_flow() ;

    if (info->owner) {

        _alloc_stk_(home, SS_MAX_PATH_LEN + 1) ;
        size_t homelen = 0 ;
        int e = errno ;
        struct passwd *st = getpwuid(info->owner) ;
        errno = 0 ;
        if (!st) {
            if (!errno) errno = ESRCH ;
            return 0 ;
        }
        errno = e ;
        if (st->pw_dir == NULL)
            log_warnusys(LOG_EXIT_ZERO, "get home directory") ;

        auto_strings(home.s, st->pw_dir) ;

        homelen = strlen(home.s) ;

        auto_strings(dst + len, "/", src) ;

        log_trace("create directory: ", dst) ;
        if (!dir_create_parent(dst, 0755))
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", dst) ;

        auto_strings(home.s + homelen, "/", src) ;

        log_trace("copy: ", home.s , " to: ", dst) ;
        if (!hiercopy(home.s, dst))
            log_warnusys_return(LOG_EXIT_ZERO, "copy: ", home.s, " to: ", dst) ;

    } else {

        auto_strings(dst + len, src) ;

        log_trace("create directory: ", dst) ;
        if (!dir_create_parent(dst, 0755))
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", dst) ;

        log_trace("copy: ", src , " to: ", dst) ;
        if (!hiercopy(src, dst))
            log_warnusys_return(LOG_EXIT_ZERO, "copy: ", src, " to: ", dst) ;
    }

    return 1 ;
}

int ssexec_snapshot_create(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    short system = 0 ;
    char const *snapname = 0 ;
    size_t pos = 0, len = 0 ;
    _alloc_stk_(snapdir, SS_MAX_PATH_LEN) ;
    _alloc_stk_(src, SS_MAX_PATH_LEN) ;
    snapshot_list_t *list = info->owner ? snapshot_user_list : snapshot_root_list ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_SNAPSHOT_CREATE, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 's' :

                    system = 1 ;
                    break ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!argc )
        log_usage(info->usage, "\n", info->help) ;

    snapname = *argv ;

    if (!system)
        if (!str_start_with(snapname, "system@"))
            log_die(LOG_EXIT_USER, "system@ is a reserved prefix for snapshot names -- please select a different one") ;

    auto_strings(snapdir.s, info->base.s, SS_SNAPSHOT + 1) ;

    len = info->base.len + (SS_SNAPSHOT_LEN - 1) ;

    if (access(snapdir.s, F_OK) < 0) {
        log_trace("create main snapshot directory: ",snapdir.s) ;
        if (!dir_create_parent(snapdir.s, 0755))
            log_dieusys(LOG_EXIT_SYS, "create directory: ",snapdir.s) ;
    }

    auto_strings(snapdir.s + len, "/", snapname) ;

    if (!access(snapdir.s, F_OK)) {

        if (!system) {
            log_dieu(LOG_EXIT_USER, "create snapshot: ", snapdir.s, " -- already exist") ;
        } else {
            log_warn("snapshot: ", snapdir.s, " already exist -- keeping it") ;
            return 0 ;
        }
    }

    len += 1 + strlen(snapname) ;

    log_trace("create snapshot directory: ", snapdir.s) ;
    if (!dir_create_parent(snapdir.s, 0755))
        log_dieusys(LOG_EXIT_SYS, "create directory: ", snapdir.s) ;

    if (!info->owner) {

        auto_strings(src.s, snapdir.s, SS_SYSTEM_DIR, SS_SYSTEM) ;

        log_trace("create directory: ", src.s) ;
        if (!dir_create_parent(src.s, 0755))
            log_dieusys(LOG_EXIT_SYS, "create directory: ", src.s) ;

        _alloc_stk_(system_dir, strlen(SS_SYSTEM_DIR) + SS_SYSTEM_LEN + 1) ;
        auto_strings(system_dir.s, SS_SYSTEM_DIR, SS_SYSTEM) ;

        log_trace("copy: ", system_dir.s , " to: ", src.s) ;
        if (!hiercopy(system_dir.s, src.s)) {
            snapshot_cleanup(snapdir.s) ;
            log_dieusys(LOG_EXIT_SYS, "copy: ", system_dir.s," to: ", src.s) ;
        }

    } else {

        auto_strings(src.s, snapdir.s, "/", SS_USER_DIR, SS_SYSTEM) ;

        log_trace("create directory: ", src.s) ;
        if (!dir_create_parent(src.s, 0755))
            log_dieusys(LOG_EXIT_SYS, "create directory: ", src.s) ;

        _alloc_stk_(system_dir, info->base.len + SS_SYSTEM_LEN + 1) ;
        auto_strings(system_dir.s, info->base.s, SS_SYSTEM) ;

        log_trace("copy: ", system_dir.s , " to: ", src.s) ;
        if (!hiercopy(system_dir.s, src.s)) {
            snapshot_cleanup(snapdir.s) ;
            log_dieusys(LOG_EXIT_SYS, "copy: ", system_dir.s," to: ", src.s) ;
        }

    }

    auto_strings(src.s, snapdir.s) ;

    while(list[pos].name) {

        if (!copy_dir(list[pos].name, src.s, len, info)) {
            snapshot_cleanup(snapdir.s) ;
            return LOG_EXIT_SYS ;
        }

        pos++ ;
    }

    log_info("Successfully created snapshot: ", snapdir.s) ;

    return 0 ;
}
