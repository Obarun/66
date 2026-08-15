/*
 * service_resolve_sanitize.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>

void service_resolve_sanitize(resolve_service_t *res)
{
    log_flow() ;

    char stk[res->sa.len + 1] ;

    memcpy(stk, res->sa.s, res->sa.len) ;
    stk[res->sa.len] = 0 ;

    res->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    resolve_init(wres) ;

    // configuration
    res->name = resolve_add_string(wres, stk + res->name) ;
    res->description = res->description ? resolve_add_string(wres, stk + res->description) : 0 ;
    res->version = res->version ? resolve_add_string(wres, stk + res->version) : 0 ;
    res->copyfrom = res->copyfrom ? resolve_add_string(wres, stk + res->copyfrom) : 0 ;
    res->intree = res->intree ? resolve_add_string(wres, stk + res->intree) : 0 ;
    res->ownerstr = res->ownerstr ? resolve_add_string(wres, stk + res->ownerstr) : 0 ;
    res->treename = res->treename ? resolve_add_string(wres, stk + res->treename) : 0 ;
    res->user = res->user ? resolve_add_string(wres, stk + res->user) : 0 ;
    res->inns = res->inns ? resolve_add_string(wres, stk + res->inns) : 0 ;

    // path
    res->path.home = res->path.home ? resolve_add_string(wres, stk + res->path.home) : 0 ;
    res->path.frontend = res->path.frontend ? resolve_add_string(wres, stk + res->path.frontend) : 0 ;
    res->path.servicedir = res->path.servicedir ? resolve_add_string(wres, stk + res->path.servicedir) : 0 ;

    // live
    res->live.livedir = res->live.livedir ? resolve_add_string(wres, stk + res->live.livedir) : 0 ;
    res->live.status = res->live.status ? resolve_add_string(wres, stk + res->live.status) : 0 ;
    res->live.servicedir = res->live.servicedir ? resolve_add_string(wres, stk + res->live.servicedir) : 0 ;
    res->live.scandir = res->live.scandir ? resolve_add_string(wres, stk + res->live.scandir) : 0 ;
    res->live.statedir = res->live.statedir ? resolve_add_string(wres, stk + res->live.statedir) : 0 ;
    res->live.eventdir = res->live.eventdir ? resolve_add_string(wres, stk + res->live.eventdir) : 0 ;
    res->live.supervisedir = res->live.supervisedir ? resolve_add_string(wres, stk + res->live.supervisedir) : 0 ;
    res->live.fdholderdir = res->live.fdholderdir ? resolve_add_string(wres, stk + res->live.fdholderdir) : 0 ;
    res->live.oneshotddir = res->live.oneshotddir ? resolve_add_string(wres, stk + res->live.oneshotddir) : 0 ;
    res->live.eventddir = res->live.eventddir ? resolve_add_string(wres, stk + res->live.eventddir) : 0 ;

    res->rversion = 0 ;

    free(wres) ;
}
