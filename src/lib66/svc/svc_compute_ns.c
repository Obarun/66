/*
 * ssexec_compute_ns.c
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
int svc_compute_ns(resolve_service_t *res, uint8_t what, ssexec_t *info, char const *updown, uint8_t opt_updown, uint8_t reloadmsg,char const *data, uint8_t propagate, pidservice_t *handled, unsigned int nhandled)
{
    log_flow() ;

    int r ;
    uint8_t requiredby = 0 ;
    size_t pos = 0 ;
    graph_t graph = GRAPH_ZERO ;
    _init_stack_(stk, SS_MAX_SERVICE * SS_MAX_SERVICE_NAME) ;

    unsigned int napid = 0 ;
    unsigned int list[SS_MAX_SERVICE + 1], visit[SS_MAX_SERVICE + 1] ;
    struct resolve_hash_s *hash = NULL ;

    memset(list, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(visit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    uint32_t gflag = STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP ;

    if (!propagate)
        FLAGS_CLEAR(gflag, STATE_FLAGS_TOPROPAGATE) ;

    if (what) {
        requiredby = 1 ;
        FLAGS_SET(gflag, STATE_FLAGS_WANTDOWN) ;
        FLAGS_CLEAR(gflag, STATE_FLAGS_WANTUP) ;
    }

    if (res->dependencies.ncontents) {

        if (!stack_clean_string_g(&stk, res->sa.s + res->dependencies.contents))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

    } else {
        log_warn("empty ns: ", res->sa.s + res->name) ;
        return 0 ;
    }

    /** build the graph of the ns */
    service_graph_g(stk.s, stk.len, &graph, &hash, info, gflag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not supervised -- initiate its first") ;

    FOREACH_STK(&stk, pos) {

        char const *name = stk.s + pos ;

        struct resolve_hash_s *h = hash_search(&hash, name) ;
        if (h == NULL)
            log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

        if (h->res.earlier) {
            log_warn("ignoring ealier service: ", h->res.sa.s + h->res.name) ;
            continue ;
        }
        graph_compute_visit(*h, visit, list, &graph, &napid, requiredby) ;
    }

    if (!what)
        sanitize_init(list, napid, &graph, &hash) ;

    pidservice_t apids[napid] ;

    svc_init_array(list, napid, apids, &graph, &hash, info, requiredby, gflag) ;

    r = svc_launch(apids, napid, what, &graph, &hash, info, updown, opt_updown, reloadmsg, data, propagate) ;

    hash_free(&hash) ;
    graph_free_all(&graph) ;

    return r ;
}
