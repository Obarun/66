/*
 * graph_compute_dependencies.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int graph_compute_dependencies(graph_t *g, char const *vertex, char const *edge, uint8_t requiredby)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    int e = 0 ;
    if (!sastr_clean_string(&sa, edge)) {
        log_warnu("clean string") ;
        goto freed ;
    }

    if (!requiredby) {

        if (!graph_vertex_add_with_nedge(g, vertex, &sa)) {
            log_warnu("add edges at vertex: ", vertex) ;
            goto freed ;
        }

    } else {

        if (!graph_vertex_add_with_nrequiredby(g, vertex, &sa)) {
            log_warnu("add requiredby at vertex: ", vertex) ;
            goto freed ;
        }
    }
    e = 1 ;

    freed:
        stralloc_free(&sa) ;
        return e ;
}
