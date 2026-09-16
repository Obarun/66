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
        resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
        resolve_wrapper_t_ref depwres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &dep) ;
        char name[strlen(c->res.sa.s + c->res.name) + 1] ;
        auto_strings(name, c->res.sa.s + c->res.name) ;

        v = hash_find(&graph.g.vertexes, name, strlen(name)) ;
        if (v == NULL)
            log_dieu(LOG_EXIT_SYS, "get information of service: ", name, " -- please make a bug report") ;

        nvertex = v->ndepends >= v->nrequiredby ? v->ndepends : v->nrequiredby ;
        _alloc_sbl_(stk, nvertex * SS_MAX_SERVICE_NAME + 1) ;

        if (!resolve_check(depwres, info->base.s, name))
            resolve_init(depwres) ;
        else if (resolve_read(depwres, info->base.s, name) <= 0)
            log_dieu(LOG_EXIT_SYS, "read dependencies addon of service: ", name) ;

        if (v->ndepends) {

            size_t pos = 0 ;
            uint8_t did = 0 ;
            _alloc_sbl_(declared, (dep.ndepends + v->ndepends) * SS_MAX_SERVICE_NAME + 1) ;
            _alloc_sbl_(led, strlen(c->dependencies.sa.s + c->dependencies.depends) + 1) ;

            if (dep.ndepends && !sbl_clean_string(&declared, dep.sa.s + dep.depends))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            if (c->dependencies.ndepends && !sbl_clean_string(&led, c->dependencies.sa.s + c->dependencies.depends))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            if (!graph_get_stkedge(&stk, &graph.g, v, false))
                log_die_nomem("strbuf") ;

            FOREACH_SBL(&stk, pos) {

                char *edge = stk.s + pos ;

                if (sbl_search(&led, edge) >= 0 || sbl_search(&declared, edge) >= 0)
                    continue ;

                if (!sbl_add(&declared, edge))
                    log_die_nomem("strbuf") ;

                did = 1 ;
            }

            if (did) {
                dep.ndepends = 0 ;
                dep.depends = parse_compute_list(depwres, &declared, &dep.ndepends, 0) ;
            }
        }

        stk.len = 0 ;

        if (v->nrequiredby && !graph_get_stkedge(&stk, &graph.g, v, true))
            log_die_nomem("strbuf") ;

        dep.nrequiredby = 0 ;
        dep.requiredby = 0 ;

        if (stk.len)
            dep.requiredby = parse_compute_list(depwres, &stk, &dep.nrequiredby, 0) ;

        c->res.has_dependencies = (dep.ndepends || dep.nrequiredby || dep.noptsdeps ||
                                   dep.ncontents || dep.nprovide || dep.nconflict) ? 1 : 0 ;

        if (!resolve_write(wres, info->base.s, name))
            log_dieu(LOG_EXIT_SYS, "write resolve file of service: ", name) ;

        if (c->res.has_dependencies && !resolve_write(depwres, info->base.s, name))
            log_dieu(LOG_EXIT_SYS, "write dependencies addon of service: ", name) ;

        resolve_free(wres) ;
        resolve_free(depwres) ;
    }
    service_graph_destroy(&graph) ;
}
