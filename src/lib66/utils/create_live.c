/*
 * create_live.c
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/types.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/tree.h>
#include <66/resolve.h>
#include <66/constants.h>

int create_live(ssexec_t *info)
{
    log_flow() ;

    int r ;

    char ownerstr[UID_FMT] ;
    size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
    size_t stelen = info->live.len + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len ;
    gid_t gidowner ;
    char ste[stelen + 1] ;

    ownerstr[ownerlen] = 0 ;

    if (!yourgid(&gidowner,info->owner))
        log_warnusys_return(LOG_EXIT_ZERO, "get gid of: ", ownerstr) ;

    auto_strings(ste, info->live.s, SS_STATE + 1, "/", ownerstr, "/", info->treename.s) ;

    r = scan_mode(ste, S_IFDIR) ;
    if (r < 0) return 0 ;
    if (!r) {

        r = dir_create_parent(ste, 0700) ;
        if (!r)
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", ste) ;

        ste[info->live.len + SS_STATE_LEN + ownerlen + 1] = 0 ;
        if (chown(ste,info->owner,gidowner) < 0)
            log_warnusys_return(LOG_EXIT_ZERO, "chown: ", ste) ;
    }

    return 1 ;
}
