/*
 * service_resolve_sanitize_addon_execute.c
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

void service_resolve_sanitize_addon_execute(resolve_service_addon_execute_t *ex)
{
    log_flow() ;

    char stk[ex->sa.len + 1] ;

    memcpy(stk, ex->sa.s, ex->sa.len) ;
    stk[ex->sa.len] = 0 ;

    ex->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EXECUTE, ex) ;

    resolve_init(wres) ;

    ex->rversion = 0 ;
    // string fields only; the integers (notify/maxdeath/timeout/down/…) are untouched
    ex->run.run = ex->run.run ? resolve_add_string(wres, stk + ex->run.run) : 0 ;
    ex->run.run_user = ex->run.run_user ? resolve_add_string(wres, stk + ex->run.run_user) : 0 ;
    ex->run.build = ex->run.build ? resolve_add_string(wres, stk + ex->run.build) : 0 ;
    ex->run.runas = ex->run.runas ? resolve_add_string(wres, stk + ex->run.runas) : 0 ;
    ex->finish.run = ex->finish.run ? resolve_add_string(wres, stk + ex->finish.run) : 0 ;
    ex->finish.run_user = ex->finish.run_user ? resolve_add_string(wres, stk + ex->finish.run_user) : 0 ;
    ex->finish.build = ex->finish.build ? resolve_add_string(wres, stk + ex->finish.build) : 0 ;
    ex->finish.runas = ex->finish.runas ? resolve_add_string(wres, stk + ex->finish.runas) : 0 ;
    ex->chdir = ex->chdir ? resolve_add_string(wres, stk + ex->chdir) : 0 ;
    ex->capsbound = ex->capsbound ? resolve_add_string(wres, stk + ex->capsbound) : 0 ;
    ex->capsambient = ex->capsambient ? resolve_add_string(wres, stk + ex->capsambient) : 0 ;

    free(wres) ;
}
