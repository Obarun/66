/*
 * service_resolve_master_copy.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stddef.h>

#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/service.h>

int service_resolve_master_copy(resolve_service_master_t *dst, resolve_service_master_t *mres)
{
    log_flow() ;

    stralloc_free(&dst->sa) ;

    size_t len = mres->sa.len - 1 ;
    dst->salen = mres->salen ;

    if (!stralloc_catb(&dst->sa,mres->sa.s,len) ||
        !stralloc_0(&dst->sa))
            return 0 ;

    dst->name = mres->name ;
    dst->classic = mres->classic ;
    dst->bundle = mres->bundle ;
    dst->longrun = mres->longrun ;
    dst->oneshot = mres->oneshot ;
    dst->module = mres->module ;

    dst->nclassic = mres->nclassic ;
    dst->nbundle = mres->nbundle ;
    dst->nlongrun = mres->nlongrun ;
    dst->noneshot = mres->noneshot ;
    dst->nmodule = mres->nmodule ;

    return 1 ;

}
