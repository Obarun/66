/*
 * svc_unsupervise.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/hash.h>
#include <oblibs/sbl.h>

#include <66/fdholder.h>

#include <66/service.h>
#include <66/state.h>
#include <66/sanitize.h>
#include <66/symlink.h>
#include <66/graph.h>
#include <66/svc.h>
#include <66/enum_parser.h>

static void sanitize_it(resolve_service_t *res, fdholder_client_t *a, uint8_t who)
{
    log_flow() ;

    if (res->has_event && !svcd_notify(res->sa.s + res->live.eventddir, 'd', who, res->sa.s + res->name))
        log_warnusys("disarm event reactor: ", res->sa.s + res->name) ;

    ss_state_t sta = STATE_ZERO ;

    if (!state_read(&sta, res))
        log_dieu(LOG_EXIT_SYS, "read state file of: ", res->sa.s + res->name) ;

    if (!sanitize_fdholder(res, a, &sta, STATE_FLAGS_FALSE, 0))
        log_warnusys("sanitize fdholder") ;

    state_set_flag(&sta, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_TRUE) ;

    if (!sanitize_scandir(res, &sta))
        log_warnusys("sanitize scandir") ;

    state_set_flag(&sta, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_TRUE) ;

    if (!sanitize_livestate(res, &sta))
        log_warnusys("sanitize livestate") ;

    if (!symlink_switch(res, SYMLINK_SOURCE))
        log_dieusys(LOG_EXIT_SYS, "switch service symlink to source for: ", res->sa.s + res->name) ;

    log_info("Unsupervised successfully: ", res->sa.s + res->name) ;
}

/** this function assume that the services is already down */
void svc_unsupervise(service_graph_t *g, uint8_t who)
{
    log_flow() ;

    uint32_t pos = 0 ;
    size_t bpos = 0 ;
    char *fdholderdir = 0 ;
    bool isstarted = false ;
    fdholder_client_t a ;

    resolve_hash_reset_visit(&g->hres) ;

    FOREACH_GRAPH_SORT(service_graph_t, g, pos) {

        uint32_t index = g->g.sort[pos] ;
        vertex_t *v = g->g.sindex[index] ;
        char *name = v->name ;

        struct resolve_hash_s *hash = resolve_hash_search(&g->hres, name) ;

        if (hash == NULL)
            log_die(LOG_EXIT_USER, "service: ", name, " not available -- please make a bug report") ;

        if (hash->visit)
            continue ;

        hash->visit = 1 ;

        fdholderdir = hash->res.sa.s + hash->res.live.fdholderdir ;

        if (!isstarted) {

            if (!sanitize_fdholder_start(&a, fdholderdir))
                log_dieu(LOG_EXIT_SYS, "start fdholder: ", fdholderdir) ;

            isstarted = true ;
        }

        sanitize_it(&hash->res, &a, who) ;

        if (hash->res.type == E_PARSER_TYPE_MODULE && hash->dependencies.ncontents) {

            bpos = 0 ;

            _alloc_sbl_(stk, strlen(hash->dependencies.sa.s + hash->dependencies.contents) + 1) ;

            if (!sbl_clean_string(&stk, hash->dependencies.sa.s + hash->dependencies.contents))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            FOREACH_SBL(&stk, bpos) {

                struct resolve_hash_s *h = resolve_hash_search(&g->hres, stk.s + bpos) ;
                if (h == NULL)
                    log_dieu(LOG_EXIT_SYS,"find hash id of: ", stk.s + bpos, " -- please make a bug reports") ;

                if (h->visit)
                    continue ;

                h->visit = 1 ;

                sanitize_it(&h->res, &a, who) ;
            }
        }
    }

    if (isstarted)
        fdholder_client_end(&a) ;
}

