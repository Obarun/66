/*
 * graph_remove_deps.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>

#include <oblibs/graph.h>
#include <oblibs/log.h>

/*
int graph_remove_deps(graph_t *g, char const *vertex, char const *edge, uint8_t requiredby)
{
    int e = 0 ;

    if (!requiredby) {

        if (!graph_edge_remove_g(g, vertex, edge))
            log_warnu_return(LOG_EXIT_ZERO, "remove edge at vertex: ", vertex) ;

    } else {

            if (!graph_edge_remove_g(g, edge, vertex))
    }

    e = 1 ;

    freed:
        return e ;
}
*/
