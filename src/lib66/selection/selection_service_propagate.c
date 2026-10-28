/*
 * selection_service_propagate.c
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

typedef struct walk_s walk_t ;
struct walk_s
{
    selection_service_t *sel ;
    selection_service_policy_t const *policy ;
    bitset32_t seen ;
    uint32_t *queue ;
    uint32_t tail ;
} ;

static void reach(walk_t *w, uint32_t index)
{
    selection_service_t *sel = w->sel ;
    service_graph_t const *g = sel->g ;

    if (bitset32_isvalid(&w->seen, index))
        return ;

    bitset32_set(&w->seen, index) ;
    w->queue[w->tail++] = index ;

    if (service_graph_state(g, index) == SERVICE_GRAPH_STATE_ABSENT) {
        bitset32_set(&sel->met, index) ;
        return ;
    }

    bitset32_set(&sel->member, index) ;

    if (w->policy->logger) {

        uint32_t logger = service_graph_logger(g, index) ;

        if (logger != SERVICE_GRAPH_NOINDEX)
            reach(w, logger) ;
    }

    uint32_t const *depends = service_graph_depends(g, index) ;
    uint8_t const *origin = service_graph_dorigin(g, index) ;

    for (uint32_t pos = 0 ; pos < service_graph_vertex(g, index)->ndepends ; pos++) {

        if (origin[pos] & SERVICE_GRAPH_EDGE_CONTENTS)
            reach(w, depends[pos]) ;
    }
}

static void walk(walk_t *w, uint32_t index)
{
    service_graph_t const *g = w->sel->g ;
    selection_service_policy_t const *policy = w->policy ;
    bool requiredby = policy->propagate == SELECTION_SERVICE_PROPAGATE_REQUIREDBY ;
    service_graph_vertex_t const *v = service_graph_vertex(g, index) ;
    uint32_t n = requiredby ? v->nrequiredby : v->ndepends ;
    uint32_t const *edges = requiredby ? service_graph_requiredby(g, index) : service_graph_depends(g, index) ;
    uint8_t const *origin = requiredby ? service_graph_rorigin(g, index) : service_graph_dorigin(g, index) ;
    bool from = requiredby ? (policy->reactors && bitset32_isvalid(policy->reactors, index)) : policy->from ;

    for (uint32_t pos = 0 ; pos < n ; pos++) {

        if (service_graph_origin_isdeclared(origin[pos]) || (from && (origin[pos] & SERVICE_GRAPH_EDGE_FROM)))
            reach(w, edges[pos]) ;
    }
}

void selection_service_propagate(selection_service_t *sel, service_graph_t const *g, bitset32_t const *seeds, selection_service_policy_t const *policy)
{
    log_flow() ;

    uint32_t n = service_graph_nvertex(g), head = 0 ;
    uint32_t queue[n + 1] ;
    bitset32_t named = *seeds ;
    walk_t w = { .sel = sel, .policy = policy, .seen = bitset32_init(n), .queue = queue, .tail = 0 } ;

    sel->g = g ;
    sel->named = named ;
    sel->member = bitset32_init(n) ;
    sel->met = bitset32_init(n) ;

    for (uint32_t index = 0 ; index < n ; index++) {

        if (bitset32_isvalid(&named, index))
            reach(&w, index) ;
    }

    if (policy->propagate == SELECTION_SERVICE_PROPAGATE_NONE)
        return ;

    while (head < w.tail)
        walk(&w, queue[head++]) ;
}
