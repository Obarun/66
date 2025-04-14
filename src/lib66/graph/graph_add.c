/*
 * graph_add.c
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

#include <errno.h>

#include <oblibs/graph2.h>
#include <oblibs/log.h>

#include <66/graph.h>

int graph_add(graph *g, const char *name)
{
    log_flow() ;

    if (!graph_add_vertex(g, name))
        return (errno = EINVAL, 0) ;

    return 1 ;
}