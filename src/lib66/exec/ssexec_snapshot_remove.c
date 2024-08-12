/*
 * ssexec_snapshot_remove.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/stack.h>
#include <oblibs/directory.h>

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/constants.h>

int ssexec_snapshot_remove(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    char const *snapname = 0 ;
    _alloc_stk_(snapdir, SS_MAX_PATH_LEN) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_SNAPSHOT_REMOVE, &l) ;
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

    log_trace("delete directory: ", snapdir.s) ;
    if (!dir_rm_rf(snapdir.s))
        log_dieusys(LOG_EXIT_SYS, "delete snapshot: ", snapdir.s) ;

    log_info("Successfully removed snapshot: ", snapname) ;

    return 0 ;
}
