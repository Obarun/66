/*
 * graph_compute_visit_g.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/stack.h>

#include <66/graph.h>
#include <66/service.h>
#include <66/enum.h>

void graph_compute_visit(resolve_service_t *ares, unsigned int aresid, unsigned int *visit, unsigned int *list, graph_t *graph, unsigned int *nservice, uint8_t requiredby)
{
    log_flow() ;

    unsigned int l[graph->mlen], c = 0, pos = 0, idx = 0 ;

    idx = graph_hash_vertex_get_id(graph, ares[aresid].sa.s + ares[aresid].name) ;

    if (!visit[idx]) {
        list[(*nservice)++] = idx ;
        visit[idx] = 1 ;
    }

    if (ares[aresid].type == TYPE_BUNDLE) {

        if (ares[idx].dependencies.ncontents) {

            size_t len = strlen(ares[aresid].sa.s + ares[aresid].dependencies.contents) ;
            _init_stack_(stk, len + 1) ;
            if (!stack_convert_string(&stk, ares[aresid].sa.s + ares[aresid].dependencies.contents, len))
                log_dieu(LOG_EXIT_SYS, "clean string") ;

            FOREACH_STK(&stk, pos) {

                idx = graph_hash_vertex_get_id(graph, stk.s + pos) ;

                if (!visit[idx]) {
                    list[(*nservice)++] = idx ;
                    visit[idx] = 1 ;
                }
            }
        }

    } else {

        /** find dependencies of the service from the graph, do it recursively */
        c = graph_matrix_get_edge_g_list(l, graph, ares[aresid].sa.s + ares[aresid].name, requiredby, 1) ;

        /** append to the list to deal with */
        for (pos = 0 ; pos < c ; pos++) {
            if (!visit[l[pos]]) {
                list[(*nservice)++] = l[pos] ;
                visit[l[pos]] = 1 ;
            }
        }
    }
}
