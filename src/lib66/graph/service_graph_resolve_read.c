/*
 * service_graph_resolve_read.c
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

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <oblibs/log.h>

#include <66/service.h>
#include <66/graph.h>

int service_graph_resolve_read(service_graph_resolve_t *store, uint32_t index, uint32_t addons, struct resolve_hash_s **h)
{
    log_flow() ;

    *h = 0 ;

    if (index >= service_graph_nvertex(store->g))
        return (errno = EINVAL, -1) ;

    uint8_t state = service_graph_state(store->g, index) ;

    if (!service_graph_readable(state))
        return (errno = EINVAL, -1) ;

    if (!store->slot[index]) {

        store->slot[index] = malloc(sizeof(struct resolve_hash_s)) ;
        if (!store->slot[index])
            return -1 ;

        *store->slot[index] = (struct resolve_hash_s)RESOLVE_HASH_ZERO ;
    }

    int r = service_read(store->slot[index], store->base, service_graph_name(store->g, index), addons) ;
    if (r <= 0)
        return r ;

    *h = store->slot[index] ;

    return 1 ;
}
