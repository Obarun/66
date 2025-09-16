/*
 * tree_init_ctx.c
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
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/graph.h>
#include <oblibs/types.h>

#include <66/tree.h>
#include <66/config.h>
#include <66/graph.h>

static tree_ctx_t ctx_init(uint32_t len)
{
    log_flow() ;

    tree_ctx_t tree = TREE_CTX_ZERO ;

    if (len > SS_MAX_SERVICE)
        log_die(LOG_EXIT_SYS, "too many tree") ;

    for (uint32_t i = 0 ; i < len; i++) {
        tree.depends[i] = NULL ;
        tree.requiredby[i] = NULL ;
    }
    tree.ndepends = tree.nrequiredby = 0 ;

    return tree ;
}


void tree_init_ctx(tree_ctx_t *atree, tree_graph_t *g, uint8_t requiredby, uint32_t flag)
{
    log_flow() ;

    vertex_t *v ;
    uint32_t pos = 0 ;
    struct resolve_hash_tree_s *hash = NULL ;

    FOREACH_GRAPH_SORT(tree_graph_t, g, pos) {

        uint32_t index = g->g.sort[pos] ;
        tree_ctx_t tree = ctx_init(g->g.nvertexes) ;
        v = g->g.sindex[index] ;
        char *name = v->name ;

        hash = hash_search_tree(&g->hres, name) ;
        if (hash == NULL)
            log_dieu(LOG_EXIT_SYS,"find hash id of: ", name, " -- please make a bug report") ;

        tree.tres = &hash->tres ;

        if (FLAGS_ISSET(flag, GRAPH_WANT_DEPENDS) || FLAGS_ISSET(flag, GRAPH_WANT_REQUIREDBY)) {

            tree.ndepends = !requiredby ? v->ndepends : v->nrequiredby ;
            graph_get_edge(&g->g, v, tree.depends, !requiredby ? false : true) ;
            tree.nrequiredby = !requiredby ? v->nrequiredby : v->ndepends ;
            graph_get_edge(&g->g, v, tree.requiredby, !requiredby ? true : false) ;
        }

        tree.index = v->index ;

        if (requiredby)
            FLAGS_SET(tree.state, TREE_FLAGS_UP) ;
        else
            FLAGS_SET(tree.state, TREE_FLAGS_DOWN) ;

        atree[pos] = tree ;
    }
}