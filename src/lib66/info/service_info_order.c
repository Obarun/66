/*
 * service_info_order.c
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

#include <66/info.h>
#include <66/graph.h>

uint32_t service_info_order(uint32_t *list, service_graph_t const *g, bitset32_t const *set, uint8_t requiredby)
{
    log_flow() ;

    uint32_t nsort = service_graph_nsort(g), n = service_graph_nvertex(g), count = 0 ;

    for (uint32_t pos = 0 ; pos < nsort ; pos++) {

        uint32_t index = service_graph_sort(g, requiredby ? nsort - 1 - pos : pos) ;

        if (bitset32_isvalid(set, index))
            list[count++] = index ;
    }

    for (uint32_t index = 0 ; index < n ; index++)
        if (bitset32_isvalid(set, index) && service_graph_state(g, index) == SERVICE_GRAPH_STATE_UNORDERED)
            list[count++] = index ;

    return count ;
}
