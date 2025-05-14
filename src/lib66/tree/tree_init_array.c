/*
 * tree_init_array.c
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

static pidtree_t pidtree_init(uint32_t len)
{
    log_flow() ;

    pidtree_t pids = PIDTREE_ZERO ;

    if (len > SS_MAX_SERVICE)
        log_die(LOG_EXIT_SYS, "too many trees") ;

    for (uint32_t i = 0 ; i < len; i++)
        pids.notif[i] = NULL ;

    return pids ;
}

void tree_init_array(pidtree_t *apidt, tree_graph_t *g, uint8_t requiredby, uint8_t flag)
{
    log_flow() ;

    vertex_t *v ;
    uint32_t pos = 0 ;
    struct resolve_hash_tree_s *hash = NULL ;

    FOREACH_GRAPH_SORT(tree_graph_t, g, pos) {

        uint32_t index = g->g.sort[pos] ;
        pidtree_t pids = pidtree_init(g->g.nvertexes) ;
        v = g->g.sindex[index] ;
        char *name = v->name ;

        hash = hash_search_tree(&g->hres, name) ;
        if (hash == NULL)
            log_dieu(LOG_EXIT_SYS,"find hash id of: ", name, " -- please make a bug report") ;

        pids.tres = &hash->tres ;

        if (FLAGS_ISSET(flag, GRAPH_WANT_DEPENDS) || FLAGS_ISSET(flag, GRAPH_WANT_REQUIREDBY)) {

            pids.nedge = !requiredby ? v->ndepends : v->nrequiredby ;
            pids.nnotif = requiredby ? v->ndepends : v->nrequiredby ;
            graph_get_edge(&g->g, v, pids.notif, requiredby ? false : true) ;
        }

        pids.index = v->index ;

        if (requiredby)
            FLAGS_SET(pids.state, TREE_FLAGS_UP) ;
        else
            FLAGS_SET(pids.state, TREE_FLAGS_DOWN) ;

        apidt[pos] = pids ;
    }
}