/*
 * set_liveenviron.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/utils.h>

int set_liveenviron(strbuf *liveenviron, uid_t owner)
{
    log_flow() ;

    char ownerpack[UID_FMT] ;
    size_t ownerlen = uid_format(ownerpack, owner) ;

    ownerpack[ownerlen] = 0 ;

    liveenviron->len = 0 ;

    if (!auto_strbuf(liveenviron, SS_LIVE, SS_LIVEENV, "/", ownerpack))
        log_warnsys_return(LOG_EXIT_ZERO, "strbuf") ;

    return 1 ;
}
