/*
 * set_livestate.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>

int set_livestate(stralloc *livestate,uid_t owner)
{
    log_flow() ;

    int r ;
    char ownerpack[UID_FMT] ;

    r = set_livedir(livestate) ;
    if (r < 0) return -1 ;
    if (!r) return 0 ;

    size_t ownerlen = uid_fmt(ownerpack,owner) ;
    ownerpack[ownerlen] = 0 ;

    if (!auto_stra(livestate,SS_STATE + 1, "/", ownerpack))
        log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    return 1 ;
}
