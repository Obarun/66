/*
 * ssexec_snapshot_restore.c
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

#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/directory.h>

#include <66/ssexec.h>
#include <66/snapshot.h>
#include <66/constants.h>

static void snapshot_remove_directory(ssexec_t *info, char const *target)
{
    size_t pos = 0 ;
    snapshot_list_t *list = info->owner ? snapshot_user_list : snapshot_root_list ;
    char stk[SS_MAX_PATH_LEN] ;

    while(list[pos].name) {

        if (!info->owner) {

            auto_strings(stk, list[pos].name) ;

        } else {

            auto_strings(stk, target, list[pos].name) ;
        }

        log_trace("remove directory: ", stk) ;
        if (!dir_destroy(stk))
            log_warnusys("remove directory: ", stk) ;

        pos++ ;
    }

    if (!info->owner) {

        auto_strings(stk, SS_SYSTEM_DIR, SS_SYSTEM) ;

    } else {

        auto_strings(stk, target, SS_USER_DIR, SS_SYSTEM) ;
    }

    log_trace("remove directory: ", stk) ;
    if (!dir_destroy(stk))
        log_warnusys("remove directory: ", stk) ;
}

int ssexec_snapshot_restore(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    size_t pos = 0, dlen = 0 ;
    char const *snapname = 0 ;
    char const *exclude[1] = { 0 } ;
    char snapdir[SS_MAX_PATH_LEN] ;
    char src[SS_MAX_PATH_LEN] ;
    char dst[SS_MAX_PATH_LEN] ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing name argument") ;

    snapname = *argv ;

    auto_strings(snapdir, info->base.s, SS_SNAPSHOT + 1, "/", snapname) ;

    if (access(snapdir, F_OK) < 0)
        log_dieusys(LOG_EXIT_SYS, "find snapshot: ", snapdir) ;

    if (!sbl_dir_get(&sa, snapdir, exclude, S_IFDIR))
        log_dieusys(LOG_EXIT_SYS, "list snapshot directory: ", snapdir) ;

    if (!info->owner) {

        auto_strings(dst, "/") ;

    } else {

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

        auto_strings(dst, st->pw_dir, "/") ;
    }

    dlen = strlen(dst) ;

    snapshot_remove_directory(info, dst) ;

    FOREACH_SBL(&sa, pos) {

        auto_strings(src, snapdir, "/", sa.s + pos) ;

        auto_strings(dst + dlen, sa.s + pos) ;

        log_trace("copy: ", src , " to: ", dst) ;
        if (!tree_copy(src, dst))
            log_dieusys(LOG_EXIT_SYS, "copy: ", src," to: ", dst) ;
    }

    log_info("Successfully restored snapshot: ", snapname) ;

    return 0 ;
}
