/*
 * selection_service_edges.c
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
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/bits.h>

#include <66/graph.h>
#include <66/selection.h>

static void keep(uint32_t *list, uint32_t *kept, uint32_t index)
{
    for (uint32_t pos = 0 ; pos < *kept ; pos++)
        if (list[pos] == index)
            return ;

    list[(*kept)++] = index ;
}

uint32_t selection_service_edges(selection_service_t const *sel, uint32_t index, uint32_t *list, bool requiredby)
{
    log_flow() ;

    uint32_t kept = 0 ;
    service_graph_vertex_t const *v = service_graph_vertex(sel->g, index) ;
    uint32_t n = requiredby ? v->nrequiredby : v->ndepends ;
    uint32_t const *edges = requiredby ? service_graph_requiredby(sel->g, index) : service_graph_depends(sel->g, index) ;

    for (uint32_t pos = 0 ; pos < n ; pos++) {

        uint32_t to = edges[pos] ;

        if (bitset32_isvalid(&sel->member, to)) {
            keep(list, &kept, to) ;
            continue ;
        }

        if (!bitset32_isvalid(&sel->met, to))
            continue ;

        service_graph_vertex_t const *a = service_graph_vertex(sel->g, to) ;
        uint32_t na = requiredby ? a->nrequiredby : a->ndepends ;
        uint32_t const *behind = requiredby ? service_graph_requiredby(sel->g, to) : service_graph_depends(sel->g, to) ;

        for (uint32_t p = 0 ; p < na ; p++) {
            if (bitset32_isvalid(&sel->member, behind[p]))
                keep(list, &kept, behind[p]) ;
        }
    }

    return kept ;
}
