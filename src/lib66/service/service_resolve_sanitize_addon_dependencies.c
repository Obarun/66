/*
 * service_resolve_sanitize_addon_dependencies.c
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

void service_resolve_sanitize_addon_dependencies(resolve_service_addon_dependencies_t *dep)
{
    log_flow() ;

    char stk[dep->sa.len + 1] ;

    memcpy(stk, dep->sa.s, dep->sa.len) ;
    stk[dep->sa.len] = 0 ;

    dep->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;

    resolve_init(wres) ;

    dep->rversion = 0 ;
    // string fields only; the n* counts are integers
    dep->depends = dep->depends ? resolve_add_string(wres, stk + dep->depends) : 0 ;
    dep->requiredby = dep->requiredby ? resolve_add_string(wres, stk + dep->requiredby) : 0 ;
    dep->optsdeps = dep->optsdeps ? resolve_add_string(wres, stk + dep->optsdeps) : 0 ;
    dep->contents = dep->contents ? resolve_add_string(wres, stk + dep->contents) : 0 ;
    dep->provide = dep->provide ? resolve_add_string(wres, stk + dep->provide) : 0 ;
    dep->conflict = dep->conflict ? resolve_add_string(wres, stk + dep->conflict) : 0 ;

    free(wres) ;
}
