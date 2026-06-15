/*
 * set_livestate.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/constants.h>
#include <66/utils.h>

int set_livestate(strbuf *livestate,uid_t owner)
{
    log_flow() ;

    int r ;
    char ownerpack[UID_FMT] ;

    r = set_livedir(livestate) ;
    if (r < 0) return -1 ;
    if (!r) return 0 ;

    size_t ownerlen = uid_format(ownerpack,owner) ;
    ownerpack[ownerlen] = 0 ;

    if (!auto_strbuf(livestate,SS_STATE + 1, "/", ownerpack))
        log_warnsys_return(LOG_EXIT_ZERO,"strbuf") ;

    return 1 ;
}
