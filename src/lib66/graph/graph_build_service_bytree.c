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
#include <66/state.h>

/*
 *
 * Need to be review entirely
 *
 * */
/** @tree: absolute path of the tree including SS_SVDIRS
 * what = 0 -> classic
 * what = 1 -> atomic and bundle
 * what > 1 -> module */
int graph_build_service_bytree(graph_t *g, char const *tree, uint8_t what, uint8_t is_supervised)
{
    log_flow() ;

    int e = 0, pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    resolve_service_master_t mres = RESOLVE_SERVICE_MASTER_ZERO ;
    resolve_wrapper_t_ref mwres = resolve_set_struct(DATA_SERVICE_MASTER, &mres) ;

    if (!resolve_read(mwres, tree, SS_MASTER + 1)) {
        log_warnu("read resolve Master service file of tree: ", tree) ;
        goto err ;
    }
    /**
     *
     *
     * a revoir ici les checks en fonction des appels qui sont fait
     * notamment par 66-inservice, 66-intree
     *
     *
     *
     * */
    if (what == 2)
        if (mres.ncontents)
            if (!auto_stra(&sa, mres.sa.s + mres.contents))
                goto err ;

    if (!what)
        if (mres.nclassic)
            if (!auto_stra(&sa, mres.sa.s + mres.classic))
                goto err ;

    if (what > 1)
        if (mres.nmodule)
            if (!auto_stra(&sa, mres.sa.s + mres.module))
                goto err ;

    if (what == 1) {

        if (mres.nbundle)
            if (!auto_stra(&sa, mres.sa.s + mres.bundle))
                goto err ;

        if (mres.noneshot)
            if (!auto_stra(&sa, mres.sa.s + mres.oneshot))
                goto err ;
    }

    if (!sastr_clean_string_flush_sa(&sa, sa.s))
        goto err ;

    FOREACH_SASTR(&sa, pos) {

        char *service = sa.s + pos ;

        if (!resolve_read(wres, tree, service))
            goto err ;

        if (is_supervised) {

            char atree[strlen(res.sa.s + res.treename) + 1] ;

            if (!service_is_g(atree, service, STATE_FLAGS_ISSUPERVISED))
                continue ;
        }

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
        resolve_free(mwres) ;
        stralloc_free(&sa) ;
        return e ;
}
