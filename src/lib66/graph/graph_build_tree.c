/*
 * graph_build_tree.c
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

#include <string.h>
#include <sys/stat.h>

#include <oblibs/graph.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/tree.h>
#include <66/graph.h>

void graph_build_tree(graph_t *g, struct resolve_hash_tree_s **htres, char const *base, resolve_tree_master_enum_t field)
{
    log_flow() ;

    size_t pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    if (!resolve_get_field_tosa_g(&sa, base, SS_MASTER + 1, DATA_TREE_MASTER, field))
        log_dieu(LOG_EXIT_SYS, "get resolve Master file of trees") ;

    FOREACH_SASTR(&sa, pos) {

        char *name = sa.s + pos ;
        struct resolve_hash_tree_s *hash = hash_search_tree(htres, name) ;

        if (hash == NULL) {

            resolve_tree_t tres = RESOLVE_TREE_ZERO ;
            resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

            if (resolve_read_g(wres, base, name) <= 0)
                log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name) ;

            if (!graph_vertex_add(g, name))
                log_dieu(LOG_EXIT_SYS, "add vertex of: ", name) ;

            if (tres.ndepends)
                if (!graph_compute_dependencies(g, name, tres.sa.s + tres.depends, 0))
                    log_dieu(LOG_EXIT_SYS, "compute dependencies of: ", name) ;

            if (tres.nrequiredby)
                if (!graph_compute_dependencies(g, name, tres.sa.s + tres.requiredby, 1))
                    log_dieu(LOG_EXIT_SYS, "compute requiredby of: ", name) ;

            log_trace("add tree: ", name, " to the graph selection") ;
            if (!hash_add_tree(htres, name, tres))
                log_dieu(LOG_EXIT_SYS, "append graph selection with: ", name) ;

            free(wres) ;
        }
    }

    if (!graph_matrix_build(g))
        log_dieu(LOG_EXIT_SYS, "build the graph") ;

    if (!graph_matrix_analyze_cycle(g))
        log_dieu(LOG_EXIT_SYS, "found cycle") ;

    if (!graph_matrix_sort(g))
        log_dieu(LOG_EXIT_SYS, "sort the graph") ;

    stralloc_free(&sa) ;
}
