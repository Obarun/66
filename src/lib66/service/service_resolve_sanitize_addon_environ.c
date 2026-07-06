/*
 * service_resolve_sanitize_addon_environ.c
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

void service_resolve_sanitize_addon_environ(resolve_service_addon_environ_t *e)
{
    log_flow() ;

    char stk[e->sa.len + 1] ;

    memcpy(stk, e->sa.s, e->sa.len) ;
    stk[e->sa.len] = 0 ;

    e->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_ENVIRON, e) ;

    resolve_init(wres) ;

    // string fields only; env_overwrite and nimportfile are integers
    e->rversion = e->rversion ? resolve_add_string(wres, stk + e->rversion) : 0 ;
    e->env = e->env ? resolve_add_string(wres, stk + e->env) : 0 ;
    e->envdir = e->envdir ? resolve_add_string(wres, stk + e->envdir) : 0 ;
    e->importfile = e->importfile ? resolve_add_string(wres, stk + e->importfile) : 0 ;

    free(wres) ;
}
