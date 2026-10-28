/*
 * service_graph_resolve_load.c
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/bits.h>

#include <66/service.h>
#include <66/graph.h>
#include <66/selection.h>

static int load(service_graph_resolve_t *store, selection_service_t const *sel, uint32_t addons, bool named)
{
    struct resolve_hash_s *h ;
    uint32_t index ;

    FOREACH_SELECTION_MEMBER(sel, index) {

        // an absent or unreadable member has no resolve file to read
        if (!service_graph_readable(service_graph_state(store->g, index)) || !bitset32_isvalid(&sel->named, index) != !named)
            continue ;

        int r = service_graph_resolve_read(store, index, addons, &h) ;

        if (!r && !named && errno == EINVAL)
            store->unreadable = index ;

        if (r <= 0)
            return r ;
    }

    return 1 ;
}

int service_graph_resolve_load(service_graph_resolve_t *store, selection_service_t const *sel, uint32_t addons)
{
    log_flow() ;

    if (sel->g != store->g)
        return (errno = EINVAL, -1) ;

    store->unreadable = SERVICE_GRAPH_NOINDEX ;

    int r = load(store, sel, addons, true) ;
    if (r <= 0)
        return r ;

    return load(store, sel, addons, false) ;
}
