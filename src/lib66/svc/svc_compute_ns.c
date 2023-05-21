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
#include <oblibs/sastr.h>

#include <skalibs/genalloc.h>

#include <66/svc.h>
#include <66/config.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/enum.h>
#include <66/service.h>
#include <66/sanitize.h>

#include <s6/supervise.h>//s6_svstatus_t

static pidservice_t pidservice_init(unsigned int len)
{
    log_flow() ;

    pidservice_t pids = PIDSERVICE_ZERO ;

    if (len > SS_MAX_SERVICE)
        log_die(LOG_EXIT_SYS, "too many services") ;

    graph_array_init_single(pids.edge, len) ;

    return pids ;
}

static void pidservice_init_array(unsigned int *list, unsigned int listlen, pidservice_t *apids, graph_t *g, resolve_service_t *ares, unsigned int areslen, ssexec_t *info, uint8_t requiredby, uint32_t flag) {

    log_flow() ;

    int r = 0 ;
    unsigned int pos = 0 ;

    for (; pos < listlen ; pos++) {

        pidservice_t pids = pidservice_init(g->mlen) ;

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[list[pos]].vertex ;

        pids.aresid = service_resolve_array_search(ares, areslen, name) ;

        if (pids.aresid < 0)
            log_dieu(LOG_EXIT_SYS,"find ares id of: ", name, " -- please make a bug reports") ;

        if (FLAGS_ISSET(flag, STATE_FLAGS_TOPROPAGATE)) {

            pids.nedge = graph_matrix_get_edge_g_sorted_list(pids.edge, g, name, requiredby, 1) ;

            if (pids.nedge < 0)
                log_dieu(LOG_EXIT_SYS,"get sorted ", requiredby ? "required by" : "dependency", " list of service: ", name) ;

            pids.nnotif = graph_matrix_get_edge_g_sorted_list(pids.notif, g, name, !requiredby, 1) ;

            if (pids.nnotif < 0)
                log_dieu(LOG_EXIT_SYS,"get sorted ", !requiredby ? "required by" : "dependency", " list of service: ", name) ;

        }

        pids.vertex = graph_hash_vertex_get_id(g, name) ;

        if (pids.vertex < 0)
            log_dieu(LOG_EXIT_SYS, "get vertex id -- please make a bug report") ;

        if (ares[pids.aresid].type != TYPE_CLASSIC) {

                ss_state_t sta = STATE_ZERO ;

                if (!state_read(&sta, &ares[pids.aresid]))
                    log_dieusys(LOG_EXIT_SYS, "read state file of: ", name) ;

                if (service_is(&sta, STATE_FLAGS_ISUP) == STATE_FLAGS_TRUE)
                    FLAGS_SET(pids.state, SVC_FLAGS_UP) ;
                else
                    FLAGS_SET(pids.state, SVC_FLAGS_DOWN) ;

        } else {

            s6_svstatus_t status ;

            r = s6_svstatus_read(ares[pids.aresid].sa.s + ares[pids.aresid].live.scandir, &status) ;

            pid_t pid = !r ? 0 : status.pid ;

            if (pid > 0) {

                FLAGS_SET(pids.state, SVC_FLAGS_UP) ;
            }
            else
                FLAGS_SET(pids.state, SVC_FLAGS_DOWN) ;
        }

        apids[pos] = pids ;
    }

}

int svc_compute_ns(resolve_service_t *res, uint8_t what, ssexec_t *info, char const *updown, uint8_t opt_updown, uint8_t reloadmsg,char const *data, uint8_t propagate)
{
    log_flow() ;

    int r ;
    uint8_t requiredby = 0 ;
    size_t pos = 0 ;
    graph_t graph = GRAPH_ZERO ;
    stralloc sa = STRALLOC_ZERO ;

    unsigned int napid = 0 ;
    unsigned int areslen = 0, list[SS_MAX_SERVICE], visit[SS_MAX_SERVICE] ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    uint32_t gflag = STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP ;

    if (!propagate)
        FLAGS_CLEAR(gflag, STATE_FLAGS_TOPROPAGATE) ;

    if (what) {
        requiredby = 1 ;
        FLAGS_SET(gflag, STATE_FLAGS_WANTDOWN) ;
        FLAGS_CLEAR(gflag, STATE_FLAGS_WANTUP) ;
    }

    if (res->dependencies.ncontents) {

        if (!sastr_clean_string(&sa, res->sa.s + res->dependencies.contents))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

    } else {
        log_warn("empty ns: ", res->sa.s + res->name) ;
        return 0 ;
    }

    /** build the graph of the ns */
    service_graph_g(sa.s, sa.len, &graph, ares, &areslen, info, gflag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not supervised -- initiate its first") ;

    graph_array_init_single(visit, SS_MAX_SERVICE) ;

    FOREACH_SASTR(&sa, pos) {

        char const *name = sa.s + pos ;

        int aresid = service_resolve_array_search(ares, areslen, name) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parsed it?") ;

        graph_compute_visit(name, visit, list, &graph, &napid, requiredby) ;

    }

    if (!what)
        sanitize_init(list, napid, &graph, ares, areslen) ;

    pidservice_t apids[napid] ;

    pidservice_init_array(list, napid, apids, &graph, ares, areslen, info, requiredby, gflag) ;

    r = svc_launch(apids, napid, what, &graph, ares, info, updown, opt_updown, reloadmsg, data, propagate) ;

    graph_free_all(&graph) ;

    service_resolve_array_free(ares, areslen) ;

    stralloc_free(&sa) ;

    return r ;
}
