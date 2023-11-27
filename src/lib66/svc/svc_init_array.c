/*
 * svc_init_array.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <66/service.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/enum.h>

#include <s6/supervise.h>

static pidservice_t pidservice_init(unsigned int len)
{
    log_flow() ;

    pidservice_t pids = PIDSERVICE_ZERO ;

    if (len > SS_MAX_SERVICE)
        log_die(LOG_EXIT_SYS, "too many services") ;

    memset(pids.edge, 0, len * sizeof(unsigned int)) ;
    memset(pids.notif, 0, len * sizeof(unsigned int)) ;

    return pids ;
}

void svc_init_array(unsigned int *list, unsigned int listlen, pidservice_t *apids, graph_t *g, resolve_service_t *ares, unsigned int areslen, ssexec_t *info, uint8_t requiredby, uint32_t flag)
{
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

                if (sta.isup == STATE_FLAGS_TRUE)
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