/*
 * svc_unsupervise.c
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

#include <oblibs/log.h>

#include <skalibs/genalloc.h>

#include <66/state.h>
#include <66/sanitize.h>
#include <66/graph.h>
#include <66/svc.h>
#include <66/enum.h>

static void sanitize_it(resolve_service_t *res)
{
    sanitize_fdholder(res, STATE_FLAGS_FALSE) ;
    sanitize_scandir(res, STATE_FLAGS_TOUNSUPERVISE) ;
    sanitize_livestate(res, STATE_FLAGS_TOUNSUPERVISE) ;
}

/** this function considers that the service is already down */
void svc_unsupervise(unsigned int *alist, unsigned int alen, graph_t *g, resolve_service_t *ares, unsigned int areslen)
{
    log_flow() ;

    unsigned int pos = 0 ;

    if (!alen)
        return ;

    for (; pos < alen ; pos++) {

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[alist[pos]].vertex ;

        int aresid = service_resolve_array_search(ares, areslen, name) ;
        if (aresid < 0)
            log_dieu(LOG_EXIT_SYS,"find ares id -- please make a bug reports") ;

        sanitize_it(&ares[aresid]) ;

        if (ares[aresid].logger.name && ares[aresid].type == TYPE_CLASSIC) {
            resolve_service_t res = RESOLVE_SERVICE_ZERO ;
            resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

            if (!resolve_read_g(wres, ares[aresid].sa.s + ares[aresid].path.home, ares[aresid].sa.s + ares[aresid].logger.name))
                log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", ares[aresid].sa.s + ares[aresid].logger.name) ;

            sanitize_it(&res) ;

            resolve_free(wres) ;
        }

        log_info("Unsupervised successfully: ", name) ;
    }
}

