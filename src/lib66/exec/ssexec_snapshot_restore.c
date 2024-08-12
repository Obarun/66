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

#include <string.h>
#include <pwd.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/stack.h>
#include <oblibs/sastr.h>
#include <oblibs/directory.h>

#include <skalibs/sgetopt.h>
#include <skalibs/djbunix.h>

#include <66/ssexec.h>
#include <66/constants.h>

int ssexec_snapshot_restore(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    size_t pos = 0 ;
    char const *snapname = 0 ;
    char const *exclude[1] = { 0 } ;
    _alloc_stk_(snapdir, SS_MAX_PATH_LEN) ;
    _alloc_stk_(src, SS_MAX_PATH_LEN) ;
    _alloc_stk_(dst, SS_MAX_PATH_LEN) ;
    _alloc_sa_(sa) ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_SNAPSHOT_RESTORE, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!argc)
        log_usage(info->usage, "\n", info->help) ;

    snapname = *argv ;

    auto_strings(snapdir.s, info->base.s, SS_SNAPSHOT + 1, "/", snapname) ;

    if (access(snapdir.s, F_OK) < 0)
        log_dieusys(LOG_EXIT_SYS, "find snapshot: ", snapdir.s) ;

    if (!sastr_dir_get(&sa, snapdir.s, exclude, S_IFDIR))
        log_dieusys(LOG_EXIT_SYS, "list snapshot directory: ", snapdir.s) ;

    FOREACH_SASTR(&sa, pos) {

        auto_strings(src.s, snapdir.s, "/", sa.s + pos) ;

        if (!info->owner) {

            auto_strings(dst.s, "/", sa.s + pos) ;

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

            auto_strings(dst.s, st->pw_dir, "/", sa.s + pos) ;
        }

        log_trace("copy: ", src.s , " to: ", dst.s) ;
        if (!hiercopy(src.s, dst.s))
            log_dieusys(LOG_EXIT_SYS, "copy: ", src.s," to: ", dst.s) ;
    }

    auto_strings(src.s, snapdir.s, "/", SS_SYSTEM, "/.version") ;

    /** remove .version file if it doesn't exist at snapshot dir.
     * Typically version under 0.8.0.0 do not have this file.*/
    if (access(src.s, F_OK) < 0) {
        auto_strings(src.s, info->base.s, SS_SYSTEM, "/.version") ;

        if (!dir_rm_rf(src.s))
            log_warnusys("remove file: ", src.s) ;
    }

    log_info("Successfully restored snapshot: ", snapname) ;

    return 0 ;
}
