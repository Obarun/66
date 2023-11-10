/*
 * ssexec_compute_ns.c
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
#include <oblibs/graph.h>
#include <oblibs/types.h>
#include <oblibs/stack.h>

#include <66/svc.h>
#include <66/config.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/service.h>
#include <66/sanitize.h>

/** sares -> services ares */
int svc_compute_ns(resolve_service_t *sares, unsigned int sareslen, unsigned int saresid, uint8_t what, ssexec_t *info, char const *updown, uint8_t opt_updown, uint8_t reloadmsg,char const *data, uint8_t propagate, pidservice_t *handled, unsigned int nhandled)
{
    log_flow() ;

    int r ;
    uint8_t requiredby = 0 ;
    size_t pos = 0 ;
    graph_t graph = GRAPH_ZERO ;
    _init_stack_(stk, SS_MAX_SERVICE * SS_MAX_SERVICE_NAME) ;

    unsigned int napid = 0 ;
    unsigned int areslen = 0, list[SS_MAX_SERVICE + 1], visit[SS_MAX_SERVICE + 1] ;
    resolve_service_t ares[SS_MAX_SERVICE + 1] ;

    memset(list, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(visit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(ares, 0, (SS_MAX_SERVICE + 1) * sizeof(resolve_service_t)) ;
    uint32_t gflag = STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP ;

    if (!propagate)
        FLAGS_CLEAR(gflag, STATE_FLAGS_TOPROPAGATE) ;

    if (what) {
        requiredby = 1 ;
        FLAGS_SET(gflag, STATE_FLAGS_WANTDOWN) ;
        FLAGS_CLEAR(gflag, STATE_FLAGS_WANTUP) ;
    }

    if (sares[saresid].dependencies.ncontents) {

        if (!stack_convert_string_g(&stk, sares[saresid].sa.s + sares[saresid].dependencies.contents))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

    } else {
        log_warn("empty ns: ", sares[saresid].sa.s + sares[saresid].name) ;
        return 0 ;
    }

    /** build the graph of the ns */
    service_graph_g(stk.s, stk.len, &graph, ares, &areslen, info, gflag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not supervised -- initiate its first") ;

    FOREACH_STK(&stk, pos) {

        char const *name = stk.s + pos ;

        int aresid = service_resolve_array_search(ares, areslen, name) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

        if (ares[aresid].earlier) {
            log_warn("ignoring ealier service: ", ares[aresid].sa.s + ares[aresid].name) ;
            continue ;
        }
        graph_compute_visit(ares, aresid, visit, list, &graph, &napid, requiredby) ;
    }

    if (!what)
        sanitize_init(list, napid, &graph, ares, areslen) ;

    pidservice_t apids[napid] ;

    svc_init_array(list, napid, apids, &graph, ares, areslen, info, requiredby, gflag) ;

    r = svc_launch(apids, napid, what, &graph, ares, areslen, info, updown, opt_updown, reloadmsg, data, propagate) ;

    graph_free_all(&graph) ;

    service_resolve_array_free(ares, areslen) ;

    return r ;
}
