/*
 * ssexec_scandir_remove.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/string.h>

#include <66/ssexec.h>
#include <66/svc.h>
#include <66/constants.h>

static void inline auto_rm(char const *str)
{
    log_flow() ;

    int r ;
    r = scan_mode(str, S_IFDIR) ;
    if (r > 0) {
        log_info("removing: ", str, "...") ;
        if (!dir_rm_rf(str))
            log_dieusys(LOG_EXIT_SYS, "remove: ", str) ;
    }
}

int ssexec_scandir_remove(int argc, char const *const *argv, ssexec_t *info)
{
    int r ;

    r = svc_scandir_ok(info->scandir.s) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check: ", info->scandir.s) ;
    if (r) {

        log_dieu(LOG_EXIT_USER, "remove: ", info->scandir.s, ": is running")  ;
        /* for now, i have a race condition
        unsigned int m = 0 ;
        int nargc = 3 ;
        char const *newargv[nargc] ;

        newargv[m++] = "stop" ;
        newargv[m++] = "stop" ;
        newargv[m] = 0 ;

        if (ssexec_scandir_signal(m, newargv, info))
            log_dieu(LOG_EXIT_SYS, "stop scandir: ", info->scandir.s) ;
        */
    }

    /** /run/66/scandir/0 */
    auto_rm(info->scandir.s) ;

    /** /run/66/scandir/container */
    info->scandir.len = 0 ;
    if (!auto_stra(&info->scandir, info->live.s, SS_SCANDIR, "/", SS_BOOT_CONTAINER_DIR))
        log_die_nomem("stralloc") ;
    auto_rm(info->scandir.s) ;

    /** run/66/state/uid */
    info->scandir.len = 0 ;
    if (!auto_stra(&info->scandir, info->live.s, SS_STATE + 1, "/", info->ownerstr))
        log_die_nomem("stralloc") ;
    auto_rm(info->scandir.s) ;

    return 0 ;
}
