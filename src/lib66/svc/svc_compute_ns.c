/*
 * ssexec_compute_ns.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <oblibs/sbl.h>
#include <oblibs/lexer.h>

#include <66/svc.h>
#include <66/graph.h>
#include <66/config.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/sanitize.h>

/** sares -> services ares */
int svc_compute_ns(svc_manager_t *mgr, uint32_t id)
{
    log_flow() ;

    svc_ctx_t *svc = &mgr->asvc[id];

    int r ;
    uint8_t requiredby = 0 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t nservice = 0, flag = GRAPH_SKIP_EARLIER ;
    _alloc_sbl_(stk, strlen(svc->res->sa.s + svc->res->dependencies.contents) + 1) ;

    if (mgr->propagate) {

        if (mgr->operation) {
            requiredby = 1 ;
            FLAGS_SET(flag, GRAPH_WANT_REQUIREDBY) ;
        } else FLAGS_SET(flag, GRAPH_WANT_DEPENDS) ;
    }

    if (svc->res->dependencies.ncontents) {

        if (!sbl_clean_string(&stk, svc->res->sa.s + svc->res->dependencies.contents))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

    } else {
        log_warn("empty ns: ", svc->res->sa.s + svc->res->name) ;
        return 0 ;
    }

    if (!graph_new(&graph, svc->res->dependencies.ncontents))
        log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

    /** build the graph of the ns */
    nservice = service_graph_build_list(&graph, stk.s, stk.len, mgr->info, flag) ;

    if (!nservice)
        log_dieu(LOG_EXIT_USER, "build the graph of the module: ", svc->res->sa.s + svc->res->name," -- please make a bug report") ;

    if (!mgr->operation)
        sanitize_init(&graph, flag) ;

    svc_ctx_t asvc[graph.g.nsort] ;

    svc_init_ctx(asvc, &graph, requiredby, flag) ;

    r = svc_launch(asvc, graph.g.nsort, mgr->operation, mgr->info, mgr->wsignal, mgr->woption, mgr->signal, mgr->cmdmsg, mgr->propagate) ;

    service_graph_destroy(&graph) ;

    return r ;
}
