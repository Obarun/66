/*
 * service_graph_resolve_get.c
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

#include <66/service.h>
#include <66/graph.h>

#include <66/svc.h>

int service_graph_resolve_get(service_graph_resolve_t *store, uint32_t index, uint32_t addons, struct resolve_hash_s **h)
{
    log_flow() ;

    struct resolve_hash_s *slot ;

    *h = 0 ;

    if (index >= service_graph_nvertex(store->g))
        return (errno = EINVAL, -1) ;

    if (service_graph_state(store->g, index) == SERVICE_GRAPH_STATE_UNREADABLE)
        return (errno = EINVAL, 0) ;

    slot = store->slot[index] ;

    if (!slot || !slot->name[0] || (slot->read & addons) != addons)
        return (errno = EINVAL, -1) ;

    *h = slot ;

    return 1 ;
}
