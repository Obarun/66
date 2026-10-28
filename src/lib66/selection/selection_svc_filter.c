/*
 * selection_svc_filter.c
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

#include <oblibs/log.h>
#include <oblibs/bits.h>

#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>

#include <66/svc.h>

int selection_svc_filter(selection_service_t *out, selection_service_t const *sel, service_graph_resolve_t *store, uint32_t addons, int (*keep)(struct resolve_hash_s const *h))
{
    log_flow() ;

    uint32_t index ;

    *out = *sel ;

    FOREACH_SELECTION_MEMBER(sel, index) {

        struct resolve_hash_s *h ;

        int r = service_graph_resolve_get(store, index, addons, &h) ;
        if (r < 0)
            return -1 ;

        if (r)
            r = keep(h) ;

        if (r < 0)
            return -1 ;

        if (!r)
            selection_service_remove(out, index) ;
    }

    return 1 ;
}
