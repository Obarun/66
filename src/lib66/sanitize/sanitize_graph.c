/*
 * sanitize_graph.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
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
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/hash.h>
#include <oblibs/string.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/graph.h>
#include <66/parse.h>

void sanitize_graph(ssexec_t *info)
{
    log_flow() ;

    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_COLLECT_PARSE|GRAPH_WANT_DEPENDS|GRAPH_WANT_REQUIREDBY, nservice = 0, nvertex = 0 ;
    struct resolve_hash_s *c, *tmp ;
    vertex_t *v = NULL ;
    resolve_wrapper_t_ref wres = 0 ;

    log_trace("sanitize system graph") ;

    if (!service_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    /** build the graph of the entire system */
    nservice = service_graph_build_system(&graph, info, flag) ;

    if (!nservice && errno == EINVAL)
        log_die(LOG_EXIT_USER, "unable to sort the system graph -- a Depends/RequiredBy cycle (or an invalid dependency) was detected; re-run with -v2 to see the offending relation") ;

    HASH_FOREACH(&graph.hres, c, tmp) {

        wres = resolve_set_struct(DATA_SERVICE, &c->res) ;
        resolve_wrapper_t_ref depwres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &c->dependencies) ;
        char name[strlen(c->res.sa.s + c->res.name) + 1] ;
        auto_strings(name, c->res.sa.s + c->res.name) ;

        v = hash_find(&graph.g.vertexes, name, strlen(name)) ;
        if (v == NULL)
            log_dieu(LOG_EXIT_SYS, "get information of service: ", name, " -- please make a bug report") ;

        nvertex = v->ndepends >= v->nrequiredby ? v->ndepends : v->nrequiredby ;
        _alloc_sbl_(stk, nvertex * SS_MAX_SERVICE_NAME + 1) ;

        if (v->ndepends) {

            if (!graph_get_stkedge(&stk, &graph.g, v, false))
                log_die_nomem("stack") ;

            c->dependencies.ndepends = 0 ;
            c->dependencies.depends = 0 ;

            if (stk.len)
                c->dependencies.depends = parse_compute_list(depwres, &stk, &c->dependencies.ndepends, 0) ;
        }

        stk.len = 0 ;

        if (v->nrequiredby) {

            if (!graph_get_stkedge(&stk, &graph.g, v, true))
                log_die_nomem("stack") ;

            c->dependencies.nrequiredby = 0 ;
            c->dependencies.requiredby = 0 ;

            if (stk.len)
                c->dependencies.requiredby = parse_compute_list(depwres, &stk, &c->dependencies.nrequiredby, 0) ;
        }

        c->res.has_dependencies = (c->dependencies.ndepends || c->dependencies.nrequiredby ||
                                   c->dependencies.noptsdeps || c->dependencies.ncontents ||
                                   c->dependencies.nprovide || c->dependencies.nconflict) ? 1 : 0 ;

        if (!resolve_write(wres, info->base.s, name))
            log_dieu(LOG_EXIT_SYS, "write resolve file of service: ", name) ;

        if (c->res.has_dependencies && !resolve_write(depwres, info->base.s, name))
            log_dieu(LOG_EXIT_SYS, "write dependencies addon of service: ", name) ;

        resolve_free(wres) ;
        free(depwres) ;
    }
    service_graph_destroy(&graph) ;
}
