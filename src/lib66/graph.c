/*
 * graph.c
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

#include <string.h>
#include <stdint.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/genalloc.h>

#include <66/graph.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/enum.h>

int graph_service_add_deps(graph_t *g, char const *service, char const *deps)
{
    stralloc sa = STRALLOC_ZERO ;
    uint8_t e = 0 ;
    if (!sastr_clean_string(&sa, deps)) {
        log_warnu("rebuild dependencies list") ;
        goto freed ;
    }

    if (!graph_vertex_add_with_nedge(g, service, &sa)) {
        log_warnu("add edges at vertex: ", service) ;
        goto freed ;
    }

    e = 1 ;

    freed:
        stralloc_free(&sa) ;
        return e ;
}

/** @tree: absolute path of the tree
 * what = 0 -> classic
 * what = 1 -> atomic and bundle
 * what > 1 -> module */
int graph_service_build_bytree(graph_t *g, char const *tree, uint8_t what)
{
    log_flow() ;

    int e = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;

    size_t treelen = strlen(tree), pos = 0 ;
    char solve[treelen + SS_SVDIRS_LEN + SS_RESOLVE_LEN + 1] ;

    auto_strings(solve, tree, SS_SVDIRS, SS_RESOLVE) ;

    char const *exclude[2] = { SS_MASTER + 1, 0 } ;

    if (!sastr_dir_get(&sa,solve,exclude,S_IFREG))
        goto err ;

    solve[treelen + SS_SVDIRS_LEN] = 0 ;

    FOREACH_SASTR(&sa, pos) {

        char *service = sa.s + pos ;

        if (!resolve_read(wres, solve, service))
            goto err ;

        char *str = res.sa.s ;

        if (!graph_vertex_add(g, service)) {
            log_warnu("add vertex: ", service) ;
            goto err ;
        }

        if (res.ndeps > 0) {

            if (res.type == TYPE_MODULE || res.type == TYPE_BUNDLE) {

                uint32_t deps = res.type == TYPE_MODULE ? what > 1 ? res.contents : res.deps : res.deps ;

                if (!graph_service_add_deps(g, service, str + deps)) {
                    log_warnu("add dependencies of service: ",service) ;
                    goto err ;
                }

            } else {

                if (!graph_service_add_deps(g, service,str + res.deps)) {
                    log_warnu("add dependencies of service: ",service) ;
                    goto err ;
                }
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
