/*
 * graph_get_stkedge.c
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
#include <errno.h>
#include <stdbool.h>

#include <oblibs/graph2.h>
#include <oblibs/stack.h>

int graph_get_stkedge(stack *stk, graph *g, vertex_t *v, bool requiredby)
{
    uint32_t nvertex = !requiredby ? v->ndepends : v->nrequiredby, pos = 0 ;
    vertex_t *vl[nvertex] ;

    graph_get_edge(g, v, vl, requiredby) ;

    for (; pos < nvertex ; pos++) {
        if (!stack_add_g(stk, vl[pos]->name))
            return (errno = ENOMEM, 0) ;
    }

    return 1 ;
}
