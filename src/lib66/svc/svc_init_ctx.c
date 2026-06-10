/*
 * svc_init_ctx.c
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
#include <oblibs/hash.h>
#include <oblibs/types.h>
#include <oblibs/sse.h>

#include <66/svc.h>
#include <66/service.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/enum_parser.h>
#include <66/graph.h>

#include <s6/supervise.h>

static svc_ctx_t ctx_init(uint32_t len)
{
    log_flow() ;

    svc_ctx_t svc = SVC_CTX_ZERO ;

    if (len > SS_MAX_SERVICE)
        log_die(LOG_EXIT_SYS, "too many services") ;

    for (uint32_t i = 0 ; i < len; i++) {
        svc.depends[i] = NULL ;
        svc.requiredby[i] = NULL ;
    }
    svc.ndepends = svc.nrequiredby = 0 ;

    return svc ;
}

void svc_init_ctx(svc_ctx_t *asvc, service_graph_t *g, uint8_t requiredby, uint32_t flag)
{
    log_flow() ;

    int r = 0 ;
    vertex_t *v ;
    uint32_t pos = 0 ;
    struct resolve_hash_s *hash = NULL ;

    FOREACH_GRAPH_SORT(service_graph_t, g, pos) {

        uint32_t index = g->g.sort[pos] ;
        svc_ctx_t svc = ctx_init(g->g.nvertexes) ;
        v = g->g.sindex[index] ;
        char *name = v->name ;

        hash = hash_search(&g->hres, name) ;
        if (hash == NULL)
            log_dieu(LOG_EXIT_SYS,"find hash id of: ", name, " -- please make a bug reports") ;

        svc.res = &hash->res ;

        if (FLAGS_ISSET(flag, GRAPH_WANT_DEPENDS) || FLAGS_ISSET(flag, GRAPH_WANT_REQUIREDBY)) {

            svc.ndepends = !requiredby ? v->ndepends : v->nrequiredby ;
            graph_get_edge(&g->g, v, svc.depends, !requiredby ? false : true) ;
            svc.nrequiredby = !requiredby ? v->nrequiredby : v->ndepends ;
            graph_get_edge(&g->g, v, svc.requiredby, !requiredby ? true : false) ;
        }

        svc.index = v->index ;

        if (svc.res->type != E_PARSER_TYPE_CLASSIC) {

                ss_state_t sta = STATE_ZERO ;

                if (!state_read(&sta, svc.res))
                    log_dieusys(LOG_EXIT_SYS, "read state file of: ", name) ;

                if (sta.isup == STATE_FLAGS_TRUE)
                    FLAGS_SET(svc.state, SVC_FLAGS_UP) ;
                else
                    FLAGS_SET(svc.state, SVC_FLAGS_DOWN) ;

        } else {

            s6_svstatus_t status ;

            r = s6_svstatus_read(svc.res->sa.s + svc.res->live.scandir, &status) ;

            pid_t pid = !r ? 0 : status.pid ;

            if (pid > 0) {

                FLAGS_SET(svc.state, SVC_FLAGS_UP) ;
            }
            else
                FLAGS_SET(svc.state, SVC_FLAGS_DOWN) ;
        }

        asvc[pos] = svc ;
    }
}
