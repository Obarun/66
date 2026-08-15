/*
 * service_resolve_sanitize_addon_event.c
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

#include <string.h>
#include <stdlib.h> // free

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>

void service_resolve_sanitize_addon_event(resolve_service_addon_event_t *ev)
{
    log_flow() ;

    char stk[ev->sa.len + 1] ;

    memcpy(stk, ev->sa.s, ev->sa.len) ;
    stk[ev->sa.len] = 0 ;

    ev->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EVENT, ev) ;

    resolve_init(wres) ;

    ev->rversion = 0 ;
    // string fields only; type/nfrom/non/combine/docmd/interval are integers
    ev->from = ev->from ? resolve_add_string(wres, stk + ev->from) : 0 ;
    ev->on = ev->on ? resolve_add_string(wres, stk + ev->on) : 0 ;
    ev->emit = ev->emit ? resolve_add_string(wres, stk + ev->emit) : 0 ;
    ev->watch = ev->watch ? resolve_add_string(wres, stk + ev->watch) : 0 ;
    ev->expression = ev->expression ? resolve_add_string(wres, stk + ev->expression) : 0 ;
    ev->timezone = ev->timezone ? resolve_add_string(wres, stk + ev->timezone) : 0 ;

    free(wres) ;
}
