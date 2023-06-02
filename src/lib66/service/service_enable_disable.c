/*
 * service_enable_disable.c
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
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/service.h>
#include <66/graph.h>
#include <66/state.h>
#include <66/utils.h>
#include <66/enum.h>

static void service_enable_disable_deps(graph_t *g, unsigned int idx, resolve_service_t *ares, unsigned int areslen, uint8_t action, visit_t *visit)
{
    log_flow() ;

    size_t pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t_ref res = &ares[idx] ;

    if (graph_matrix_get_edge_g_sa(&sa, g, res->sa.s + res->name, action ? 0 : 1, 0) < 0)
        log_dieu(LOG_EXIT_SYS, "get ", action ? "dependencies" : "required by" ," of: ", res->sa.s + res->name) ;

    if (sa.len) {

        FOREACH_SASTR(&sa, pos) {

            char *name = sa.s + pos ;
            int aresid = service_resolve_array_search(ares, areslen, name) ;
            if (aresid < 0)
                log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

            if (visit[aresid] == VISIT_WHITE)
                service_enable_disable(g, aresid, ares, areslen, action, visit) ;
        }
    }

    stralloc_free(&sa) ;
}

/** @action -> 0 disable
 * @action -> 1 enable */
void service_enable_disable(graph_t *g, unsigned int idx, resolve_service_t *ares, unsigned int areslen, uint8_t action, visit_t *visit)
{
    log_flow() ;

    if (visit[idx] == VISIT_WHITE) {

        resolve_service_t_ref res = &ares[idx] ;

        if (!state_messenger(res, STATE_FLAGS_ISENABLED, !action ? STATE_FLAGS_FALSE : STATE_FLAGS_TRUE))
            log_dieusys(LOG_EXIT_SYS, "send message to state of: ", res->sa.s + res->name) ;

        service_enable_disable_deps(g, idx, ares, areslen, action, visit) ;

        /** the logger must be disabled to avoid to start it
         * with the 66 tree start <tree> command */
        if (res->logger.want && !action && res->type == TYPE_CLASSIC && !res->inmodule) {

            char *name = res->sa.s + res->logger.name ;

            int aresid = service_resolve_array_search(ares, areslen, name) ;
            if (aresid < 0)
                log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

            if (visit[aresid] == VISIT_WHITE) {

                if (!state_messenger(&ares[aresid], STATE_FLAGS_ISENABLED, !action ? STATE_FLAGS_FALSE : STATE_FLAGS_TRUE))
                    log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

                log_info("Disabled successfully service: ", name) ;

                visit[aresid] = VISIT_GRAY ;
            }
        }

        if (res->type == TYPE_MODULE || res->type == TYPE_BUNDLE) {

            if (res->dependencies.ncontents) {

                size_t pos = 0 ;
                stralloc sa = STRALLOC_ZERO ;

                if (!sastr_clean_string(&sa, res->sa.s + res->dependencies.contents))
                    log_dieu(LOG_EXIT_SYS, "clean string") ;

                FOREACH_SASTR(&sa, pos) {

                    char *name = sa.s + pos ;
                    int aresid = service_resolve_array_search(ares, areslen, name) ;
                    if (aresid < 0)
                        log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

                    if (visit[aresid] == VISIT_WHITE) {

                        if (!state_messenger(&ares[aresid], STATE_FLAGS_ISENABLED, !action ? STATE_FLAGS_FALSE : STATE_FLAGS_TRUE))
                            log_dieusys(LOG_EXIT_SYS, "send message to state of: ", res->sa.s + res->name) ;

                        service_enable_disable_deps(g, aresid, ares, areslen, action, visit) ;

                        visit[aresid] = VISIT_GRAY ;

                        log_info(!action ? "Disabled" : "Enabled"," successfully service: ", ares[aresid].sa.s + ares[aresid].name) ;
                    }
                }

                stralloc_free(&sa) ;
            }
        }

        visit[idx] = VISIT_GRAY ;

        log_info(!action ? "Disabled" : "Enabled"," successfully service: ", res->sa.s + res->name) ;
    }
}
