/*
 * svc_unsupervise.c
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

#include <oblibs/log.h>
#include <oblibs/sastr.h>

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

    sanitize_fdholder(res, &sta, STATE_FLAGS_FALSE) ;

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
void svc_unsupervise(unsigned int *alist, unsigned int alen, graph_t *g, resolve_service_t *ares, unsigned int areslen, ssexec_t *info)
{
    log_flow() ;

    unsigned int pos = 0 ;
    size_t bpos = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    if (!alen)
        return ;

    for (; pos < alen ; pos++) {

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[alist[pos]].vertex ;

        int aresid = service_resolve_array_search(ares, areslen, name) ;
        if (aresid < 0)
            log_dieu(LOG_EXIT_SYS,"find ares id of: ", name, " -- please make a bug reports") ;

        sanitize_it(&ares[aresid]) ;

        if (ares[aresid].type == TYPE_MODULE && ares[aresid].dependencies.ncontents) {

            sa.len = 0, bpos = 0 ;

            if (!sastr_clean_string(&sa, ares[aresid].sa.s + ares[aresid].dependencies.contents))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            FOREACH_SASTR(&sa, bpos) {

                int aresid = service_resolve_array_search(ares, areslen, sa.s + bpos) ;
                if (aresid < 0)
                    log_dieu(LOG_EXIT_SYS,"find ares id of: ", sa.s + bpos, " -- please make a bug reports") ;

                sanitize_it(&ares[aresid]) ;
            }
        }
    }
    stralloc_free(&sa) ;
}

