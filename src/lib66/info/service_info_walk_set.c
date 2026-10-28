/*
 * service_info_walk_set.c
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

int service_info_walk_set(service_graph_t const *g, bitset32_t const *set, uint8_t requiredby, uint8_t reverse, depth_t *depth, int padding, info_graph_style *style)
{
    log_flow() ;

    uint32_t list[service_graph_nvertex(g) + 1] ;
    uint32_t count ;

    if ((unsigned int) depth->level > INFO_MAXDEPTH)
        return 1 ;

    count = service_info_order(list, g, set, requiredby) ;

    for (uint32_t pos = 0 ; reverse && pos < count / 2 ; pos++) {
        uint32_t t = list[pos] ;
        list[pos] = list[count - 1 - pos] ;
        list[count - 1 - pos] = t ;
    }

    for (uint32_t idx = 0 ; idx < count ; idx++) {

        uint32_t v = list[idx] ;
        int last = idx + 1 < count ? 0 : 1 ;
        service_graph_vertex_t const *vertex = service_graph_vertex(g, v) ;

        if (!info_graph_display(service_graph_name(g, v), &info_graph_display_service, depth, last, padding, style))
            return 0 ;

        if (!requiredby ? vertex->ndepends : vertex->nrequiredby) {

            depth_t d = {
                depth,
                NULL,
                depth->level + 1
            } ;
            depth->next = &d;

            if (last) {

                if (depth->prev) {

                    depth->prev->next = &d;
                    d.prev = depth->prev;
                    depth = &d;
                }
                else
                    d.prev = NULL;
            }

            // below the first level, the tree a service belongs to does not filter
            if (!service_info_walk(g, v, requiredby, reverse, &d, padding, style))
                return 0 ;

            depth->next = NULL ;
        }
    }

    return 1 ;
}
