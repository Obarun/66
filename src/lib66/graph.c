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

int graph_add_deps(graph_t *g, char const *vertex, char const *edge, uint8_t requiredby)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    int e = 0 ;
    if (!sastr_clean_string(&sa, edge)) {
        log_warnu("rebuild dependencies list") ;
        goto freed ;
    }

    if (!requiredby) {

        if (!graph_vertex_add_with_nedge(g, vertex, &sa)) {
            log_warnu("add edges at vertex: ", vertex) ;
            goto freed ;
        }

    } else {

        if (!graph_vertex_add_with_nrequiredby(g, vertex, &sa)) {
            log_warnu("add requiredby at vertex: ", vertex) ;
            goto freed ;
        }
    }
    e = 1 ;

    freed:
        stralloc_free(&sa) ;
        return e ;
}

/*
int graph_remove_deps(graph_t *g, char const *vertex, char const *edge, uint8_t requiredby)
{
    int e = 0 ;

    if (!requiredby) {

        if (!graph_edge_remove_g(g, vertex, edge))
            log_warnu_return(LOG_EXIT_ZERO, "remove edge at vertex: ", vertex) ;

    } else {

            if (!graph_edge_remove_g(g, edge, vertex))
    }

    e = 1 ;

    freed:
        return e ;
}
*/

/**
 *
 *
 *
 *
 * a revoir auto_strings(solve, base, SS_SYSTEM, SS_RESOLVE) ; c'est faux pour les services
 *
 *
 *
 *
 * */

int graph_build_g(graph_t *g, char const *base, char const *treename, uint8_t data_type)
{
    log_flow() ;

    if (data_type == DATA_SERVICE) {

        //if (!graph_build_service(g, base, treename))
            return 0 ;

    } else if (data_type == DATA_TREE) {

        //if (!graph_build_tree(g, base))
            return 0 ;
    }

    if (!graph_matrix_build(g))
        log_warnu_return(LOG_EXIT_ZERO, "build the graph") ;

    if (!graph_matrix_analyze_cycle(g))
        log_warnu_return(LOG_EXIT_ZERO, "found cycle") ;

    if (!graph_matrix_sort(g))
        log_warnu_return(LOG_EXIT_ZERO, "sort the graph") ;

    return 1 ;
}

int graph_build(graph_t *g, char const *base, char const *treename, uint8_t what)
{
    log_flow() ;

    int e = 0 ;
    size_t baselen = strlen(base), treelen = strlen(treename), pos = 0 ;
    char const *exclude[2] = { SS_MASTER + 1, 0 } ;
    char solve[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + 1 + treelen + 1] ;

    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

    RESOLVE_SET_SAWRES(wres) ;

    auto_strings(solve, base, SS_SYSTEM, SS_RESOLVE) ;

    if (!sastr_dir_get(&sa, solve, exclude, S_IFREG))
        goto err ;

    solve[baselen + SS_SYSTEM_LEN] = 0 ;

    FOREACH_SASTR(&sa, pos) {

        char *name = sa.s + pos ;

        if (!resolve_read(wres, solve, name))
            goto err ;

        if (what == DATA_SERVICE) {

            auto_strings(solve + baselen + SS_SYSTEM_LEN, "/", treename) ;

            if (!graph_build_service_bytree(g, solve, 2))
                goto err ;

        } else if (what == DATA_TREE) {

            if (!graph_vertex_add(g, name))
                goto err ;

            if (tres.ndepends)
                if (!graph_add_deps(g, name, sawres->s + tres.depends, 0))
                    goto err ;

            if (tres.nrequiredby)
                if (!graph_add_deps(g, name, sawres->s + tres.requiredby, 1))
                    goto err ;
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

/** @tree: absolute path of the tree
 * what = 0 -> classic
 * what = 1 -> atomic and bundle
 * what > 1 -> module */
int graph_build_service_bytree(graph_t *g, char const *tree, uint8_t what)
{
    log_flow() ;

    int e = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    size_t treelen = strlen(tree), pos = 0 ;
    char solve[treelen + SS_SVDIRS_LEN + SS_RESOLVE_LEN + 1] ;

    auto_strings(solve, tree, SS_SVDIRS, SS_RESOLVE) ;

    char const *exclude[2] = { SS_MASTER + 1, 0 } ;

    if (!sastr_dir_get(&sa,solve,exclude,S_IFREG))
        goto err ;

    solve[treelen + SS_SVDIRS_LEN] = 0 ;

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
        stralloc_free(&sa) ;
        return e ;
}
