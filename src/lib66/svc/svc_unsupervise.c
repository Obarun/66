/*
 * svc_unsupervise.c
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

#include <oblibs/log.h>
#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <66/state.h>
#include <66/sanitize.h>
#include <66/graph.h>
#include <66/svc.h>
#include <66/enum.h>
#include <66/symlink.h>
#include <66/constants.h>

static void sanitize_it(resolve_service_t *res)
{
    log_flow() ;

    ss_state_t sta = STATE_ZERO ;

    if (!state_read(&sta, res))
        log_dieu(LOG_EXIT_SYS, "read state file of: ", res->sa.s + res->name) ;

    sanitize_fdholder(res, &sta, STATE_FLAGS_FALSE, 0) ;

    svc_send_fdholder(res->sa.s + res->live.fdholderdir, "twR") ;

    state_set_flag(&sta, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_TRUE) ;
    state_set_flag(&sta, STATE_FLAGS_ISUP, STATE_FLAGS_FALSE) ;

    sanitize_scandir(res, &sta) ;

    state_set_flag(&sta, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_TRUE) ;

    sanitize_livestate(res, &sta) ;

    if (!symlink_switch(res, SYMLINK_SOURCE))
        log_dieusys(LOG_EXIT_SYS, "switch service symlink to source for: ", res->sa.s + res->name) ;

    log_info("Unsupervised successfully: ", res->sa.s + res->name) ;
}

/** this function considers that the service is already down except for the logger */
void svc_unsupervise(unsigned int *alist, unsigned int alen, graph_t *g, struct resolve_hash_s **hres, ssexec_t *info)
{
    log_flow() ;

    unsigned int pos = 0 ;
    size_t bpos = 0 ;

    if (!alen)
        return ;

    for (; pos < alen ; pos++) {

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[alist[pos]].vertex ;

        struct resolve_hash_s *hash = hash_search(hres, name) ;
        if (hash == NULL)
            log_dieu(LOG_EXIT_SYS,"find hash id of: ", name, " -- please make a bug reports") ;

        sanitize_it(&hash->res) ;

        if (hash->res.type == TYPE_MODULE && hash->res.dependencies.ncontents) {

            bpos = 0 ;

            _alloc_stk_(stk, strlen(hash->res.sa.s + hash->res.dependencies.contents) + 1) ;

            if (!stack_string_clean(&stk, hash->res.sa.s + hash->res.dependencies.contents))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            FOREACH_STK(&stk, bpos) {

                struct resolve_hash_s *h = hash_search(hres, stk.s + bpos) ;
                if (h == NULL)
                    log_dieu(LOG_EXIT_SYS,"find hash id of: ", stk.s + bpos, " -- please make a bug reports") ;

                sanitize_it(&h->res) ;
            }
        }
    }
}

