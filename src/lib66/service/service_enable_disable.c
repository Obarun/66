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

typedef enum visit_service_e visit_service_t ;
enum visit_service_e
{
    SS_WHITE = 0,
    SS_GRAY,
    SS_BLACK
} ;

static void visit_init(visit_service_t *v, size_t len)
{
    log_flow() ;

    size_t pos = 0 ;
    for (; pos < len; pos++)
        v[pos] = SS_WHITE ;

}

static void service_enable_disable_deps(graph_t *g, char const *base, char const *sv, uint8_t action)
{
    log_flow() ;

    size_t pos = 0, element = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    if (graph_matrix_get_edge_g_sa(&sa, g, sv, action ? 0 : 1, 0) < 0)
        log_dieu(LOG_EXIT_SYS, "get ", action ? "dependencies" : "required by" ," of: ", sv) ;

    size_t len = sastr_nelement(&sa) ;
    visit_service_t v[len] ;

    visit_init(v, len) ;

    if (sa.len) {

        FOREACH_SASTR(&sa, pos) {

            if (v[element] == SS_WHITE) {

                char *name = sa.s + pos ;

                service_enable_disable(g, base, name, action) ;

                v[element] = SS_GRAY ;
            }
            element++ ;
        }
    }

    stralloc_free(&sa) ;
}

/** @action -> 0 disable
 * @action -> 1 enable */
void service_enable_disable(graph_t *g, char const *base, char const *name, uint8_t action)
{
    log_flow() ;

    if (!state_messenger(base, name, STATE_FLAGS_ISENABLED, !action ? STATE_FLAGS_FALSE : STATE_FLAGS_TRUE))
        log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

    service_enable_disable_deps(g, base, name, action) ;

    log_info(!action ? "Disabled" : "Enabled"," successfully service: ", name) ;

}
