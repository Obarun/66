/*
 * graph_build_service_bytree_from_src.c
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
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/graph.h>

/** @tsrc: absolute path of the tree including SS_SVDIRS
 * what = 0 -> classic
 * what = 1 -> atomic and bundle
 * what > 1 -> module */
int graph_build_service_bytree_from_src(graph_t *g, char const *src, uint8_t what)
{
    log_flow() ;

    int e = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    size_t srclen = strlen(src), pos = 0 ;
    char solve[srclen + SS_RESOLVE_LEN + 1] ;

    auto_strings(solve, src, SS_RESOLVE) ;

    char const *exclude[2] = { SS_MASTER + 1, 0 } ;

    if (!sastr_dir_get(&sa,solve,exclude,S_IFREG))
        goto err ;

    solve[srclen] = 0 ;

    /** can be needed for 66-inservice and 66-intree.
     * TODO: try to avoid it
     * */
    if (!service_resolve_sort_bytype(&sa, solve))
        goto err ;

    FOREACH_SASTR(&sa, pos) {

        char *service = sa.s + pos ;

        if (!resolve_read(wres, solve, service))
            goto err ;

        char *str = res.sa.s ;

        if (!graph_vertex_add(g, service)) {
            log_warnu("add vertex: ", service) ;
            goto err ;
        }

        if (res.dependencies.ndepends) {

            if (!graph_compute_dependencies(g, service,str + res.dependencies.depends, 0)) {
                log_warnu("add dependencies of service: ",service) ;
                goto err ;
            }
        }

        if (res.dependencies.nrequiredby) {

            if (!graph_compute_dependencies(g, service, str + res.dependencies.requiredby, 1)) {
                log_warnu("add requiredby of service: ", service) ;
                goto err ;
            }
        }
    }

    if (!graph_matrix_build(g)) {
        log_warnu("build the graph") ;
        goto err ;
    }

    if (!graph_matrix_analyze_cycle(g)) {
        log_warn("found cycle") ;
        goto err ;
    }

    if (!graph_matrix_sort(g)) {
        log_warnu("sort the graph") ;
        goto err ;
    }

    e = 1 ;

    err:
        resolve_free(wres) ;
        stralloc_free(&sa) ;
        return e ;
}

