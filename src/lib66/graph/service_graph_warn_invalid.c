/*
 * service_graph_warn_invalid.c
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
#include <oblibs/strbuf.h>
#include <oblibs/bits.h>

#include <66/graph.h>
#include <66/service.h>

int service_graph_warn_invalid(service_graph_t const *g, strbuf *frontends, bitset32_t const *skip)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    uint32_t n = service_graph_nvertex(g) ;

    for (uint32_t index = 0 ; index < n ; index++) {

        bool gone = false ;
        uint8_t state = service_graph_state(g, index) ;

        if ((skip && bitset32_isvalid(skip, index)) || state == SERVICE_GRAPH_STATE_OK)
            continue ;

        if (state == SERVICE_GRAPH_STATE_ABSENT) {

            int r = service_graph_frontend_gone(g, index, frontends) ;
            if (r < 0)
                return -1 ;

            gone = r ;
        }

        int r = service_graph_invalid_message(&sa, g, index, gone) ;
        if (r < 0)
            return -1 ;

        if (r)
            log_warn(sa.s) ;
    }

    return 1 ;
}
