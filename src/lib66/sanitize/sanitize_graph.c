/*
 * sanitize_graph.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/graph.h>

#include <skalibs/stralloc.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/parser.h>
#include <66/state.h>
#include <66/graph.h>

/** rewrite depends/requiredby of each service
 * found on the system */
void sanitize_graph(ssexec_t *info)
{
    log_flow() ;

    int n = 0 ;
    uint32_t flag = 0 ;
    unsigned int areslen = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t ares[SS_MAX_SERVICE] ;
    graph_t graph = GRAPH_ZERO ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_TOPARSE|STATE_FLAGS_WANTUP|STATE_FLAGS_WANTDOWN) ;

    /** build the graph of the entire system */
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    for (; n < areslen ; n++) {

        sa.len = 0 ;
        resolve_service_t_ref res = &ares[n] ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

        char *name = res->sa.s + res->name ;

        if (graph_matrix_get_edge_g_sa(&sa, &graph, name, 0, 1) < -1)
            log_dieu(LOG_EXIT_SYS, "get dependencies of service: ", name) ;

        res->dependencies.ndepends = 0 ;
        res->dependencies.depends = 0 ;

        if (sa.len)
            res->dependencies.depends = parse_compute_list(wres, &sa, &res->dependencies.ndepends, 0) ;

        sa.len = 0 ;

        if (graph_matrix_get_edge_g_sa(&sa, &graph, name, 1, 1) < -1)
            log_dieu(LOG_EXIT_SYS, "get requiredby of service: ", name) ;

        res->dependencies.nrequiredby = 0 ;
        res->dependencies.requiredby = 0 ;

        if (sa.len)
            res->dependencies.requiredby = parse_compute_list(wres, &sa, &res->dependencies.nrequiredby, 0) ;

        if (!resolve_write_g(wres, info->base.s, name))
            log_dieu(LOG_EXIT_SYS, "write resolve file of service: ", name) ;

        free(wres) ;
    }

    service_resolve_array_free(ares, areslen) ;
    graph_free_all(&graph) ;
    stralloc_free(&sa) ;
}
