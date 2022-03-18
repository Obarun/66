/*
 * sa_pointo.c
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
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/utils.h>

int sa_pointo(stralloc *sa, ssexec_t *info, int type, unsigned int where)
{
    log_flow() ;

    sa->len = 0 ;

    char ownerstr[UID_FMT] ;
    size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
    ownerstr[ownerlen] = 0 ;

    if (where == SS_RESOLVE_STATE) {

        if (!auto_stra(sa, info->live.s, SS_STATE + 1, "/", ownerstr, "/", info->treename.s))
            goto err ;

    } else if (where == SS_RESOLVE_SRC) {

        if (!auto_stra(sa, info->tree.s, SS_SVDIRS))
            goto err ;

    } else if (where == SS_RESOLVE_BACK) {

        if (!auto_stra(sa, info->base.s, SS_SYSTEM, SS_BACKUP, "/", info->treename.s))
            goto err ;

    } else if (where == SS_RESOLVE_LIVE) {

        if (!auto_stra(sa, info->live.s, SS_STATE + 1, "/", ownerstr, "/", info->treename.s, SS_SVDIRS))
            goto err ;
    }

    if (type >= 0 && where) {

        if (type == TYPE_CLASSIC) {

            if (!auto_stra(sa, SS_SVC))
                goto err ;

        } else if (!auto_stra(sa, SS_DB))
            goto err ;
    }

    return 1 ;

    err:
        return 0 ;
}
