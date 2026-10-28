/*
 * selection_service_names.c
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
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/bits.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>

int selection_service_names(bitset32_t *named, service_graph_t const *g, char const *base, char const *const *argv, int argc, bool provide, int *unknown)
{
    log_flow() ;

    *named = bitset32_init(service_graph_nvertex(g)) ;

    for (int i = 0 ; i < argc ; i++) {

        char buf[SS_MAX_SERVICE_NAME + 1] ;
        char const *name = argv[i] ;

        if (provide) {

            int r = service_resolve_provide(buf, name, 0, base) ;
            if (r < 0)
                return -1 ;

            if (r)
                name = buf ;
        }

        uint32_t index = service_graph_find(g, name) ;

        if (index == SERVICE_GRAPH_NOINDEX) {
            *unknown = i ;
            return 0 ;
        }

        if (!bitset32_isvalid(named, index))
            bitset32_set(named, index) ;
    }

    return 1 ;
}
