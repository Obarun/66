/*
 * service_resolve_copy.c
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

#include <stddef.h>

#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/service.h>

int service_resolve_copy(resolve_service_t *dst, resolve_service_t *res)
{
    log_flow() ;

    stralloc_free(&dst->sa) ;

    size_t len = res->sa.len - 1 ;
    dst->salen = res->salen ;

    if (!stralloc_catb(&dst->sa,res->sa.s,len) ||
        !stralloc_0(&dst->sa))
            return 0 ;

    dst->name = res->name ;
    dst->description = res->description ;
    dst->version = res->version ;
    dst->logger = res->logger ;
    dst->logreal = res->logreal ;
    dst->logassoc = res->logassoc ;
    dst->dstlog = res->dstlog ;
    dst->depends = res->depends ;
    dst->requiredby = res->requiredby ;
    dst->optsdeps = res->optsdeps ;
    dst->extdeps = res->extdeps ;
    dst->contents = res->contents ;
    dst->src = res->src ;
    dst->srconf = res->srconf ;
    dst->live = res->live ;
    dst->runat = res->runat ;
    dst->tree = res->tree ;
    dst->treename = res->treename ;
    dst->state = res->state ;
    dst->exec_run = res->exec_run ;
    dst->exec_log_run = res->exec_log_run ;
    dst->real_exec_run = res->real_exec_run ;
    dst->real_exec_log_run = res->real_exec_log_run ;
    dst->exec_finish = res->exec_finish ;
    dst->real_exec_finish = res->real_exec_finish ;
    dst->type = res->type ;
    dst->ndepends = res->ndepends ;
    dst->nrequiredby = res->nrequiredby ;
    dst->noptsdeps = res->noptsdeps ;
    dst->nextdeps = res->nextdeps ;
    dst->ncontents = res->ncontents ;
    dst->down = res->down ;
    dst->disen = res->disen ;

    return 1 ;
}
