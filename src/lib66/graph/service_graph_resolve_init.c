/*
 * service_graph_resolve_init.c
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

#include <oblibs/log.h>

#include <66/service.h>
#include <66/graph.h>

int service_graph_resolve_init(service_graph_resolve_t *store, service_graph_t const *g, char const *base)
{
    log_flow() ;

    uint32_t n = service_graph_nvertex(g) ;

    store->g = g ;
    store->base = base ;
    store->slot = 0 ;
    store->unreadable = SERVICE_GRAPH_NOINDEX ;

    if (!n)
        return 1 ;

    store->slot = calloc(n, sizeof(struct resolve_hash_s *)) ;
    if (!store->slot)
        return -1 ;

    return 1 ;
}
