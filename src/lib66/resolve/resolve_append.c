/*
 * resolve_append.c
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

#include <oblibs/log.h>

#include <skalibs/genalloc.h>

#include <66/resolve.h>
#include <66/graph.h>
#include <66/service.h>
#include <66/tree.h>

int resolve_append(genalloc *ga, resolve_wrapper_t *wres)
{
    log_flow() ;

    int e = 0 ;

    if (wres->type == DATA_SERVICE) {

        resolve_service_t cp = RESOLVE_SERVICE_ZERO ;
        if (!service_resolve_copy(&cp, ((resolve_service_t *)wres->obj)))
            goto err ;

        if (!genalloc_append(resolve_service_t, ga, &cp))
            goto err ;

    } else if (wres->type == DATA_TREE) {

        resolve_tree_t cp = RESOLVE_TREE_ZERO ;
        if (!tree_resolve_copy(&cp, ((resolve_tree_t *)wres->obj)))
            goto err ;

        if (!genalloc_append(resolve_tree_t, ga, &cp))
            goto err ;

    } else if (wres->type == DATA_TREE_MASTER) {

            resolve_tree_master_t cp = RESOLVE_TREE_MASTER_ZERO ;

            if (!tree_resolve_master_copy(&cp, ((resolve_tree_master_t *)wres->obj)))
                goto err ;

            if (!genalloc_append(resolve_tree_master_t, ga, &cp))
                goto err ;
    }

    e = 1 ;
    err:
        return e ;
}
