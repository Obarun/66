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
#include <oblibs/opt.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>

#include <66/snapshot.h>
#include <66/ssexec.h>
#include <66/constants.h>

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
    if (!dir_destroy(dir))
        log_warnu("remove service directory: ", dir) ;
}

static int copy_dir(const char *src, char *dst, size_t len, ssexec_t *info)
{
    log_flow() ;

    if (info->owner) {

        char home[SS_MAX_PATH_LEN + 1] ;
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

        auto_strings(home, st->pw_dir) ;

        homelen = strlen(home) ;

        auto_strings(dst + len, "/", src) ;

        log_trace("create directory: ", dst) ;
        if (!dir_create_parent(dst, 0755))
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", dst) ;

        auto_strings(home + homelen, "/", src) ;

        log_trace("copy: ", home , " to: ", dst) ;
        if (!tree_copy(home, dst))
            log_warnusys_return(LOG_EXIT_ZERO, "copy: ", home, " to: ", dst) ;

    } else {

        auto_strings(dst + len, src) ;

        log_trace("create directory: ", dst) ;
        if (!dir_create_parent(dst, 0755))
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", dst) ;

        log_trace("copy: ", src , " to: ", dst) ;
        if (!tree_copy(src, dst))
            log_warnusys_return(LOG_EXIT_ZERO, "copy: ", src, " to: ", dst) ;
    }

    return 1 ;
}

static short opt_system = 0 ;

int on_snapshot_create(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {

        case 's' :

            opt_system = 1 ;
            break ;
    }

    return 0 ;
}

int ssexec_snapshot_create(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    short system = opt_system ;
    /* drain option state into the local, then reset the static for re-entrancy. */
    opt_system = 0 ;
    char const *snapname = 0 ;
    size_t pos = 0, len = 0 ;
    char snapdir[SS_MAX_PATH_LEN] ;
    char src[SS_MAX_PATH_LEN] ;
    snapshot_list_t *list = info->owner ? snapshot_user_list : snapshot_root_list ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing name argument") ;

    snapname = *argv ;

    if (!system)
        if (!str_start_with(snapname, "system@"))
            log_die(LOG_EXIT_USER, "system@ is a reserved prefix for snapshot names -- please select a different one") ;

    auto_strings(snapdir, info->base.s, SS_SNAPSHOT + 1) ;

    len = info->base.len + (SS_SNAPSHOT_LEN - 1) ;

    if (access(snapdir, F_OK) < 0) {
        log_trace("create main snapshot directory: ",snapdir) ;
        if (!dir_create_parent(snapdir, 0755))
            log_dieusys(LOG_EXIT_SYS, "create directory: ",snapdir) ;
    }

    auto_strings(snapdir + len, "/", snapname) ;

    if (!access(snapdir, F_OK)) {

        if (!system) {
            log_dieu(LOG_EXIT_USER, "create snapshot: ", snapdir, " -- already exist") ;
        } else {
            log_warn("snapshot: ", snapdir, " already exist -- keeping it") ;
            return 0 ;
        }
    }

    len += 1 + strlen(snapname) ;

    log_trace("create snapshot directory: ", snapdir) ;
    if (!dir_create_parent(snapdir, 0755))
        log_dieusys(LOG_EXIT_SYS, "create directory: ", snapdir) ;

    if (!info->owner) {

        auto_strings(src, snapdir, SS_SYSTEM_DIR, SS_SYSTEM) ;

        log_trace("create directory: ", src) ;
        if (!dir_create_parent(src, 0755))
            log_dieusys(LOG_EXIT_SYS, "create directory: ", src) ;

        char system_dir[strlen(SS_SYSTEM_DIR) + SS_SYSTEM_LEN + 1] ;
        auto_strings(system_dir, SS_SYSTEM_DIR, SS_SYSTEM) ;

        log_trace("copy: ", system_dir , " to: ", src) ;
        if (!tree_copy(system_dir, src)) {
            snapshot_cleanup(snapdir) ;
            log_dieusys(LOG_EXIT_SYS, "copy: ", system_dir," to: ", src) ;
        }

    } else {

        auto_strings(src, snapdir, "/", SS_USER_DIR, SS_SYSTEM) ;

        log_trace("create directory: ", src) ;
        if (!dir_create_parent(src, 0755))
            log_dieusys(LOG_EXIT_SYS, "create directory: ", src) ;

        char system_dir[info->base.len + SS_SYSTEM_LEN + 1] ;
        auto_strings(system_dir, info->base.s, SS_SYSTEM) ;

        log_trace("copy: ", system_dir , " to: ", src) ;
        if (!tree_copy(system_dir, src)) {
            snapshot_cleanup(snapdir) ;
            log_dieusys(LOG_EXIT_SYS, "copy: ", system_dir," to: ", src) ;
        }

    }

    auto_strings(src, snapdir) ;

    while(list[pos].name) {

        if (!copy_dir(list[pos].name, src, len, info)) {
            snapshot_cleanup(snapdir) ;
            return LOG_EXIT_SYS ;
        }

        pos++ ;
    }

    log_info("Successfully created snapshot: ", snapdir) ;

    return 0 ;
}
