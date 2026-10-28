/*
 * service_graph_find.c
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
#include <string.h>

#include <oblibs/log.h>

#include <66/graph.h>

uint32_t service_graph_find(service_graph_t const *g, char const *name)
{
    log_flow() ;

    uint32_t lo = 0, hi = service_graph_nvertex(g) ;

    while (lo < hi) {

        uint32_t mid = lo + (hi - lo) / 2 ;
        int c = strcmp(name, service_graph_name(g, mid)) ;

        if (!c)
            return mid ;

        if (c < 0)
            hi = mid ;
        else
            lo = mid + 1 ;
    }

    return SERVICE_GRAPH_NOINDEX ;
}
