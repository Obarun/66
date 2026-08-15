/*
 * service_resolve_sanitize_addon_limit.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h> // free

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>

void service_resolve_sanitize_addon_limit(resolve_service_addon_limit_t *l)
{
    log_flow() ;

    char stk[l->sa.len + 1] ;

    memcpy(stk, l->sa.s, l->sa.len) ;
    stk[l->sa.len] = 0 ;

    l->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_LIMIT, l) ;

    resolve_init(wres) ;

    l->rversion = 0 ;

    free(wres) ;
}
