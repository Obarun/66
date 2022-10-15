/*
 * graph_build_service_from_list.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/graph.h>

#include <skalibs/genalloc.h>

#include <66/config.h>
#include <66/graph.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/state.h>

int graph_build_service_from_list(char const *const *list, char const *base, graph_t *graph, resolve_service_t *ares, uint8_t requiredby)
{
    log_flow() ;


    unsigned int areslen = 0, e = 0 ;
    char atree[SS_MAX_TREENAME + 1] ;

    for (; *list ; list++) {

        int found = 0 ;
        unsigned int pos = 0, ndeps = 0 ;
        unsigned int  alist[graph->mlen] ;
        char const *name = *list ;

        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

        graph_array_init_single(alist, graph->mlen) ;

        resolve_service_t cp = RESOLVE_SERVICE_ZERO ;

        if (!resolve_read_g(wres, base, name))
            goto err ;

        if (!service_resolve_copy(&cp, &res))
            goto err ;

        if (service_resolve_array_search(ares, areslen, name) < 0)
            ares[areslen++] = cp ;

        ndeps = graph_matrix_get_edge_g_list(alist, graph, name, requiredby, 1) ;
        if (ndeps < 0)
            goto err ;

        for (; pos < ndeps ; pos++) {

            char *name = graph->data.s + genalloc_s(graph_hash_t, &graph->hash)[alist[pos]].vertex ;

            if (service_resolve_array_search(ares, areslen, name) < 0) {

                resolve_service_t cp = RESOLVE_SERVICE_ZERO ;

                found = service_is_g(atree, name, STATE_FLAGS_ISPARSED) ;
                if (found <= 0)
                    goto err ;

                if (!resolve_read_g(wres, base, name))
                    goto err ;

                if (!service_resolve_copy(&cp, &res))
                    goto err ;

                ares[areslen++] = cp ;
            }
        }
        resolve_free(wres) ;
    }

    e = areslen ;

    err:

        return e ;
}
