/*
 * tree_graph_destroy.c
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

#include <66/tree.h>
#include <66/graph.h>

void tree_graph_destroy(tree_graph_t *g)
{
    graph_free(&g->g) ;
    hash_free_tree(&g->hres) ;
}


