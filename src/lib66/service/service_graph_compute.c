/*
 * service_graph_compute.c
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
#include <oblibs/types.h>
#include <oblibs/graph.h>
#include <oblibs/stack.h>
#include <oblibs/string.h>
#include <oblibs/lexer.h>

#include <66/service.h>
#include <66/graph.h>
#include <66/state.h>
#include <66/hash.h>

static void issupervised(char *store, struct resolve_hash_s **hres, char const *str)
{
    size_t pos = 0, len = strlen(str) ;
    ss_state_t ste = STATE_ZERO ;

    _alloc_stk_(stk, len + 1) ;
    memset(store, 0, len * sizeof(char)) ;

    if (!stack_string_clean(&stk, str))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    FOREACH_STK(&stk, pos) {

        char *name = stk.s + pos ;

        struct resolve_hash_s *hash = hash_search(hres, name) ;
        if (hash == NULL) {
            log_warn("service: ", name, " not available -- ignore it") ;
            continue ;
        }

        if (!state_check(&hash->res))
            continue ;

        if (!state_read(&ste, &hash->res))
            continue ;

        if (ste.issupervised == STATE_FLAGS_TRUE)
            auto_strings(store + strlen(store), name, " ") ;
        else
            continue ;
    }

    store[strlen(store) - 1] = 0 ;
}

void service_graph_compute(graph_t *g, struct resolve_hash_s **hres, uint32_t flag)
{
    log_flow() ;

    ss_state_t ste = STATE_ZERO ;
    resolve_service_t_ref pres = 0 ;
    struct resolve_hash_s *c, *tmp ;

    HASH_ITER(hh, *hres, c, tmp) {

        pres = &c->res ;
        char *service = pres->sa.s + pres->name ;

        if (!state_check(pres))
            continue ;

        if (!state_read(&ste, pres))
            continue ;

        if (ste.issupervised == STATE_FLAGS_FALSE && FLAGS_ISSET(flag, STATE_FLAGS_ISSUPERVISED)) {
            log_warn("requested flags issupervised where service ", service, " is not -- ignoring it") ;
            continue ;
        }

        if (!graph_vertex_add(g, service))
            log_dieu(LOG_EXIT_SYS, "add vertex: ", service) ;

        if (FLAGS_ISSET(flag, STATE_FLAGS_TOPROPAGATE)) {

            if (pres->dependencies.ndepends && FLAGS_ISSET(flag, STATE_FLAGS_WANTUP)) {

                char store[strlen(pres->sa.s + pres->dependencies.depends) + 1 * pres->dependencies.ndepends] ;

                if (FLAGS_ISSET(flag, STATE_FLAGS_ISSUPERVISED)) {

                    issupervised(store, hres, pres->sa.s + pres->dependencies.depends) ;

                } else {

                    auto_strings(store, pres->sa.s + pres->dependencies.depends) ;

                }

                if (strlen(store))
                    if (!graph_compute_dependencies(g, service, store, 0))
                        log_dieu(LOG_EXIT_SYS, "add dependencies of service: ",service) ;
            }

            if (pres->dependencies.nrequiredby && FLAGS_ISSET(flag, STATE_FLAGS_WANTDOWN)) {

                char store[strlen(pres->sa.s + pres->dependencies.requiredby) + 1 * pres->dependencies.nrequiredby] ;

                if (FLAGS_ISSET(flag, STATE_FLAGS_ISSUPERVISED)) {

                    issupervised(store, hres, pres->sa.s + pres->dependencies.requiredby) ;

                } else {

                    auto_strings(store, pres->sa.s + pres->dependencies.requiredby) ;
                }

                if (strlen(store))
                    if (!graph_compute_dependencies(g, service, store, 1))
                        log_dieu(LOG_EXIT_SYS, "add requiredby of service: ",service) ;
            }
        }
    }

    if (!graph_matrix_build(g))
        log_dieu(LOG_EXIT_SYS, "build the graph") ;

    if (!graph_matrix_analyze_cycle(g))
        log_die(LOG_EXIT_SYS, "cyclic graph detected") ;

    if (!graph_matrix_sort(g))
        log_dieu(LOG_EXIT_SYS, "sort the graph") ;
}
