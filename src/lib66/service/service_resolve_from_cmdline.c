/*
 * resolve_from_cmdline.c
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
#include <oblibs/string.h>
#include <oblibs/graph.h>

#include <skalibs/genalloc.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/ssexec.h>
#include <66/constants.h>

int service_resolve_from_cmdline(resolve_service_t *ares, graph_t *graph, ssexec_t *info, char const *const *argv, uint8_t requiredby)
{

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;
    unsigned int areslen = 0 ;

    for (; *argv ; argv++) {

        unsigned int pos = 0, *alist = 0, ndeps = 0 ;
        char const *name = *argv ;

        wres = resolve_set_struct(DATA_SERVICE, &res) ;

        if (resolve_read_g(wres, info->base.s, name) <= 0)
            log_dieusys(LOG_EXIT_SYS,"read resolve file of: ", name) ;

        resolve_service_t cp = RESOLVE_SERVICE_ZERO ;

        if (!service_resolve_copy(&cp, &res))
            log_dieusys(LOG_EXIT_SYS,"copy resolve file of: ", name) ;

        if (service_resolve_array_search(ares, areslen, name) < 0)
            ares[areslen++] = cp ;

        ndeps = graph_matrix_get_edge_g_list(alist, graph, name, requiredby, 1) ;

        if (ndeps < 0)
            log_dieu(LOG_EXIT_SYS, "get dependencies of service: ", name) ;

        for (; pos < ndeps ; pos++) {

            char *name = graph->data.s + genalloc_s(graph_hash_t, &graph->hash)[alist[pos]].vertex ;

            resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
            wres = resolve_set_struct(DATA_SERVICE, &dres) ;

            if (!service_resolve_array_search(ares, areslen, name)) {

                if (resolve_read_g(wres, info->base.s, name) <= 0)
                    log_dieusys(LOG_EXIT_SYS,"read resolve file of: ",name) ;

                ares[areslen++] = dres ;
            }

            resolve_free(wres) ;
        }

        resolve_free(wres) ;
    }


    return areslen ;
}
