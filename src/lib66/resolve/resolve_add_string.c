/*
 * resolve_add_string.c
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

#include <string.h>
#include <sys/types.h>//ssize_t

#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/constants.h>
#include <66/service.h>
#include <66/tree.h>

ssize_t resolve_add_string(resolve_wrapper_t *wres, char const *data)
{
    log_flow() ;

    RESOLVE_SET_SAWRES(wres) ;

    ssize_t baselen = sawres->len ;

    if (!data) {

        if (!stralloc_catb(sawres,"",1))
            log_warnusys_return(LOG_EXIT_LESSONE,"stralloc") ;

        return baselen ;
    }

    size_t datalen = strlen(data) ;

    if (!stralloc_catb(sawres,data,datalen + 1))
        log_warnusys_return(LOG_EXIT_LESSONE,"stralloc") ;

    return baselen ;
}
