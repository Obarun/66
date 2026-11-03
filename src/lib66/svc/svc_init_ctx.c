/*
 * svc_init_ctx.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/bits.h>

#include <66/svc.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/selection.h>

static int member_of_reactor(selection_service_t const *ordered, service_graph_resolve_t *store, struct resolve_hash_s const *h)
{
    struct resolve_hash_s *module ;

    if (!h->res.inns)
        return 0 ;

    uint32_t index = service_graph_find(store->g, h->res.sa.s + h->res.inns) ;

    if (index == SERVICE_GRAPH_NOINDEX || !bitset32_isvalid(&ordered->member, index))
        return 0 ;

    if (service_graph_resolve_get(store, index, SERVICE_READ_EVENT, &module) <= 0)
        return -1 ;

    return svc_reactor_armed_idle(module) > 0 ;
}

static int ctx_of(svc_ctx_t *asvc, uint32_t *n, selection_service_t const *ordered, service_graph_resolve_t *store, uint32_t index, bool reverse, bool unordered)
{
    svc_ctx_t svc = SVC_CTX_ZERO ;
    struct resolve_hash_s *h ;

    if (service_graph_resolve_get(store, index, SERVICE_READ_EXECUTE | SERVICE_READ_EVENT, &h) <= 0)
        return -1 ;

    int supervised = svc_issupervised(&h->res) ;
    if (supervised < 0)
        return -1 ;

    int r = supervised ? svc_isrunning_supervised(h) : 0 ;
    if (r < 0)
        return -1 ;

    int reactor = member_of_reactor(ordered, store, h) ;
    if (reactor < 0)
        return -1 ;

    svc.h = h ;
    svc.index = index ;
    svc.supervised = supervised ;
    svc.mreactor = reactor ;
    FLAGS_SET(svc.state, r ? SVC_FLAGS_UP : SVC_FLAGS_DOWN) ;

    if (!unordered) {

        uint32_t edges[ordered->member.count] ;

        svc.ndepends = selection_service_edges(ordered, index, edges, reverse) ;
        svc.nrequiredby = selection_service_edges(ordered, index, edges, !reverse) ;
    }

    asvc[(*n)++] = svc ;

    return 1 ;
}

int svc_init_ctx(svc_ctx_t *asvc, selection_service_t const *lot, service_graph_resolve_t *store, svc_request_t const *req)
{
    log_flow() ;

    uint32_t pos = 0, index = 0, n = 0 ;
    bool reverse = req->action != SVC_ACTION_START ;
    selection_service_t ordered ;

    selection_service_ordered(&ordered, lot) ;

    if (!reverse && ordered.member.count != lot->member.count)
        return (errno = EINVAL, -1) ;

    if (reverse) {

        FOREACH_SELECTION_UNORDERED(lot, index)
            if (ctx_of(asvc, &n, &ordered, store, index, reverse, true) < 0)
                return -1 ;

        FOREACH_SELECTION_REVERSE(lot, pos, index)
            if (ctx_of(asvc, &n, &ordered, store, index, reverse, false) < 0)
                return -1 ;

    } else {

        FOREACH_SELECTION(lot, pos, index)
            if (ctx_of(asvc, &n, &ordered, store, index, reverse, false) < 0)
                return -1 ;
    }

    if (n != lot->member.count)
        return (errno = EINVAL, -1) ;

    return 1 ;
}
