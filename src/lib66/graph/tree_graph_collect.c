/*
 * tree_graph_collect.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/lexer.h>

#include <66/graph.h>
#include <66/ssexec.h>
#include <66/tree.h>
#include <66/resolve.h>


uint32_t tree_graph_ncollect(tree_graph_t *g, const char *list, size_t len, ssexec_t *info)
{
    log_flow() ;

    uint32_t n = 0 ;
    size_t pos = 0 ;

    for (; pos < len ; pos += strlen(list + pos) + 1)
        n += tree_graph_collect(g, list + pos, info) ;

    return n ;
}

uint32_t tree_graph_collect(tree_graph_t *g, const char *treename, ssexec_t *info)
{
    log_flow() ;

    uint32_t n = 0 ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    struct resolve_hash_tree_s *hash = NULL ;

    hash = hash_search_tree(&g->hres, treename) ;

    if (hash == NULL) {

        if (resolve_read_g(wres, info->base.s, treename) <= 0)
            log_dieu(LOG_EXIT_SYS, "read resolve file of: ", treename) ;

        log_trace("add tree: ", treename, " to the tree selection") ;
        if (!hash_add_tree(&g->hres, treename, tres))
            log_dieu(LOG_EXIT_SYS, "append graph selection with: ", treename) ;

        n++ ;

        if (tres.ndepends) {

            size_t len = strlen(tres.sa.s + tres.depends) ;
            _alloc_stk_(stk, len + 1) ;

            if (!stack_string_clean(&stk, tres.sa.s + tres.depends))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            n += tree_graph_ncollect(g, stk.s, stk.len, info) ;
        }

        if (tres.nrequiredby) {

            size_t len = strlen(tres.sa.s + tres.requiredby) ;
            _alloc_stk_(stk, len + 1) ;

            if (!stack_string_clean(&stk, tres.sa.s + tres.requiredby))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            n += tree_graph_ncollect(g, stk.s, stk.len, info) ;
        }
    }

    free(wres) ;

    return n ;
}
