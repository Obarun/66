/*
 * sanitize_graph.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/parse.h>
#include <66/state.h>
#include <66/graph.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/hash.h>

/** rewrite depends/requiredby of each service
 * found on the system */
void sanitize_graph(ssexec_t *info)
{
    log_flow() ;

    uint32_t flag = 0 ;
    _alloc_sa_(sa) ;
    struct resolve_hash_s *hres = NULL, *c, *tmp ;
    graph_t graph = GRAPH_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_TOPARSE|STATE_FLAGS_WANTUP|STATE_FLAGS_WANTDOWN) ;

    log_trace("sanitize system graph") ;

    /** build the graph of the entire system */
    graph_build_system(&graph, &hres, info, flag) ;

    HASH_ITER(hh, hres, c, tmp) {

        sa.len = 0 ;

        wres = resolve_set_struct(DATA_SERVICE, &c->res) ;
        char name[strlen(c->res.sa.s + c->res.name) + 1] ;
        auto_strings(name, c->res.sa.s + c->res.name) ;

        if (graph_matrix_get_edge_g_sa(&sa, &graph, name, 0, 0) < 0)
            log_dieu(LOG_EXIT_SYS, "get dependencies of service: ", name) ;

        c->res.dependencies.ndepends = 0 ;
        c->res.dependencies.depends = 0 ;

        if (sa.len) {
            _alloc_stk_(stk, sa.len + 1) ;
            if (!stack_copy(&stk, sa.s, sa.len))
                log_die_nomem("stack overflow") ;

            c->res.dependencies.depends = parse_compute_list(wres, &stk, &c->res.dependencies.ndepends, 0) ;
        }

        sa.len = 0 ;

        if (graph_matrix_get_edge_g_sa(&sa, &graph, name, 1, 0) < 0)
            log_dieu(LOG_EXIT_SYS, "get requiredby of service: ", name) ;

        c->res.dependencies.nrequiredby = 0 ;
        c->res.dependencies.requiredby = 0 ;

        if (sa.len) {
            _alloc_stk_(stk, sa.len + 1) ;
            if (!stack_copy(&stk, sa.s, sa.len))
                log_die_nomem("stack overflow") ;

            c->res.dependencies.requiredby = parse_compute_list(wres, &stk, &c->res.dependencies.nrequiredby, 0) ;
        }

        if (!resolve_write_g(wres, info->base.s, name))
            log_dieu(LOG_EXIT_SYS, "write resolve file of service: ", name) ;

        resolve_free(wres) ;
    }

    hash_free(&hres) ;
    graph_free_all(&graph) ;
}
