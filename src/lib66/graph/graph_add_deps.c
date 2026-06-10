/*
 * graph_add_deps.c
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

#include <stdbool.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/lexer.h>
#include <oblibs/graph.h>

#include <66/graph.h>

int graph_add_deps(graph *g, const char *vertex, char const *edge, bool requiredby)
{
    log_flow() ;

    _alloc_sbl_(stk, strlen(edge) + 1) ;

    if (!sbl_clean_string(&stk, edge))
        log_warnu_return(LOG_EXIT_ZERO, "clean string") ;

    if (!graph_add_nedge(g, vertex, &stk, requiredby, true))
        log_warnu_return(LOG_EXIT_ZERO, "add edges to vertex: ", vertex) ;

    return 1 ;
}