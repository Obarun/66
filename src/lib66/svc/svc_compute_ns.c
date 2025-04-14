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
#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <66/svc.h>
#include <66/graph.h>
#include <66/config.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/sanitize.h>

/** sares -> services ares */
int svc_compute_ns(resolve_service_t *res, uint8_t what, ssexec_t *info, char const *updown, uint8_t opt_updown, uint8_t reloadmsg,char const *data, uint8_t propagate)
{
    log_flow() ;

    int r ;
    uint8_t requiredby = 0 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t nservice = 0, flag = GRAPH_SKIP_EARLIER ;
    _alloc_stk_(stk, strlen(res->sa.s + res->dependencies.contents) + 1) ;

    if (propagate) {

        if (what) {
            requiredby = 1 ;
            FLAGS_SET(flag, GRAPH_WANT_REQUIREDBY) ;
        } else FLAGS_SET(flag, GRAPH_WANT_DEPENDS) ;
    }

    if (res->dependencies.ncontents) {

        if (!stack_string_clean(&stk, res->sa.s + res->dependencies.contents))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

    } else {
        log_warn("empty ns: ", res->sa.s + res->name) ;
        return 0 ;
    }

    if (!graph_new(&graph, res->dependencies.ncontents))
        log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

    /** build the graph of the ns */
    nservice = service_graph_build_list(&graph, stk.s, stk.len, info, flag) ;

    if (!nservice)
        log_dieu(LOG_EXIT_USER, "build the graph of the module: ", res->sa.s + res->name," -- please make a bug report") ;

    if (!what)
        sanitize_init(&graph, flag) ;

    pidservice_t apids[graph.g.nsort] ;

    svc_init_array(apids, &graph, requiredby, flag) ;

    r = svc_launch(apids, graph.g.nsort, what, info, updown, opt_updown, reloadmsg, data, propagate) ;

    service_graph_destroy(&graph) ;

    return r ;
}
