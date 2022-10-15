/*
 * service_graph_build.c
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

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/graph.h>

#include <66/service.h>
#include <66/graph.h>
#include <66/state.h>

void service_graph_build(graph_t *g, resolve_service_t *ares, unsigned int areslen, uint32_t flag)
{
    log_flow() ;

    unsigned int pos = 0 ;
    resolve_service_t_ref pres = 0 ;

    for (; pos < areslen ; pos++) {

        pres = &ares[pos] ;
        char *service = pres->sa.s + pres->name ;

        if (!graph_vertex_add(g, service))
            log_dieu(LOG_EXIT_SYS, "add vertex: ", service) ;

        if (FLAGS_ISSET(flag, STATE_FLAGS_TOPROPAGATE)) {

            if (pres->dependencies.ndepends && FLAGS_ISSET(flag, STATE_FLAGS_WANTUP))
                if (!graph_compute_dependencies(g, service, pres->sa.s + pres->dependencies.depends, 0))
                    log_dieu(LOG_EXIT_SYS, "add dependencies of service: ",service) ;

            if (pres->dependencies.nrequiredby && FLAGS_ISSET(flag, STATE_FLAGS_WANTDOWN))
                if (!graph_compute_dependencies(g, service, pres->sa.s + pres->dependencies.requiredby, 1))
                    log_dieu(LOG_EXIT_SYS, "add dependencies of service: ",service) ;
        }
    }

    if (!graph_matrix_build(g))
        log_dieu(LOG_EXIT_SYS, "build the graph") ;

    if (!graph_matrix_analyze_cycle(g))
        log_dieu(LOG_EXIT_SYS, "found cycle") ;

    if (!graph_matrix_sort(g))
        log_dieu(LOG_EXIT_SYS, "sort the graph") ;
}
