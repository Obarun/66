/*
 * service_graph_resolve_free.c
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

void service_graph_resolve_free(service_graph_resolve_t *store)
{
    log_flow() ;

    if (store->slot) {

        uint32_t n = service_graph_nvertex(store->g) ;

        for (uint32_t index = 0 ; index < n ; index++) {

            if (!store->slot[index])
                continue ;

            service_read_free(store->slot[index]) ;
            free(store->slot[index]) ;
        }

        free(store->slot) ;
    }

    *store = (service_graph_resolve_t)SERVICE_GRAPH_RESOLVE_ZERO ;
}
