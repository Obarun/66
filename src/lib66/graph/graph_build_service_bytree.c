/*
 * graph_build_service_bytree.c
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

#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/graph.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/constants.h>
#include <66/service.h>
#include <66/enum.h>
#include <66/graph.h>

/** @tree: absolute path of the tree including SS_SVDIRS
 * what = 0 -> classic
 * what = 1 -> atomic and bundle
 * what > 1 -> module */
int graph_build_service_bytree(graph_t *g, char const *tree, uint8_t what)
{
    log_flow() ;

    int e = 0, pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    resolve_service_master_t mres = RESOLVE_SERVICE_MASTER_ZERO ;
    resolve_wrapper_t_ref mwres = resolve_set_struct(DATA_SERVICE_MASTER, &mres) ;

    if (!resolve_read_g(mwres, tree, SS_MASTER + 1)) {
        log_warnu("read resolve Master file of trees") ;
        goto err ;
    }

    if (mres.nclassic)
        if (!auto_stra(&sa, mres.sa.s + mres.classic))
            goto err ;

    if (mres.nmodule)
        if (!auto_stra(&sa, mres.sa.s + mres.module))
            goto err ;

    if (mres.nbundle)
        if (!auto_stra(&sa, mres.sa.s + mres.bundle))
            goto err ;

    if (mres.nlongrun)
        if (!auto_stra(&sa, mres.sa.s + mres.longrun))
            goto err ;

    if (mres.noneshot)
        if (!auto_stra(&sa, mres.sa.s + mres.oneshot))
            goto err ;

    if (!sastr_clean_string_g(&sa, sa.s))
        goto err ;

    FOREACH_SASTR(&sa, pos) {

        char *service = sa.s + pos ;

        if (!resolve_read(wres, tree, service))
            goto err ;

        char *str = res.sa.s ;

        if (!graph_vertex_add(g, service)) {
            log_warnu("add vertex: ", service) ;
            goto err ;
        }

        if (res.ndepends) {

            if (res.type == TYPE_MODULE || res.type == TYPE_BUNDLE) {

                uint32_t depends = res.type == TYPE_MODULE ? what > 1 ? res.contents : res.depends : res.depends ;

                if (!graph_add_deps(g, service, str + depends, 0)) {
                    log_warnu("add dependencies of service: ",service) ;
                    goto err ;
                }

            } else {

                if (!graph_add_deps(g, service,str + res.depends, 0)) {
                    log_warnu("add dependencies of service: ",service) ;
                    goto err ;
                }
            }
        }

        if (res.nrequiredby) {

            if (!graph_add_deps(g, service, str + res.requiredby, 1)) {
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
        resolve_free(mwres) ;
        stralloc_free(&sa) ;
        return e ;
}
