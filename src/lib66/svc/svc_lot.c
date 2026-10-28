/*
 * svc_lot.c
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/bits.h>

#include <66/svc.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>

static int holds_only_earlier(selection_service_t const *lot, uint32_t index)
{
    service_graph_t const *g = lot->g ;
    uint32_t ncontents = 0, kept = 0, pos = 0 ;
    uint32_t const *edges = service_graph_depends(g, index) ;
    uint8_t const *origin = service_graph_dorigin(g, index) ;

    for (; pos < service_graph_vertex(g, index)->ndepends ; pos++) {

        if (!(origin[pos] & SERVICE_GRAPH_EDGE_CONTENTS))
            continue ;

        ncontents++ ;

        if (bitset32_isvalid(&lot->member, edges[pos]))
            kept++ ;
    }

    return ncontents && !kept ;
}

int svc_lot(selection_service_t *lot, selection_service_t const *sel, service_graph_resolve_t *store, uint8_t stage)
{
    log_flow() ;

    service_graph_t const *g = sel->g ;
    uint32_t n = service_graph_nvertex(g) ;

    if (stage > SVC_LOT_EARLIER)
        return (errno = EINVAL, -1) ;

    *lot = *sel ;

    if (stage == SVC_LOT_BOOT)
        return 1 ;

    uint32_t index ;

    FOREACH_SELECTION_MEMBER(sel, index) {

        struct resolve_hash_s *h ;

        int r = service_graph_resolve_get(store, index, 0, &h) ;
        if (r < 0)
            return -1 ;

        if (!r) {
            if (stage == SVC_LOT_EARLIER)
                selection_service_remove(lot, index) ;
            continue ;
        }

        if (stage == SVC_LOT_EARLIER) {

            if (!h->res.earlier)
                selection_service_remove(lot, index) ;

        } else if (h->res.earlier && selection_service_carried(sel, index))
            selection_service_remove(lot, index) ;
    }

    if (stage == SVC_LOT_EARLIER)
        return 1 ;

    for (uint32_t index = 0 ; index < n ; index++) {

        if (!bitset32_isvalid(&lot->member, index) || !holds_only_earlier(lot, index))
            continue ;

        log_warn("the module: ", service_graph_name(g, index), " holds only earlier services -- nothing to act on outside the boot stage") ;
        selection_service_remove(lot, index) ;
    }

    return 1 ;
}
