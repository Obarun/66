/*
 * graph_build_g.c
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

#include <oblibs/graph.h>
#include <oblibs/log.h>

#include <66/service.h>
#include <66/tree.h>
#include <66/graph.h>

int graph_build_g(graph_t *g, char const *base, char const *treename, uint8_t data_type, uint8_t general)
{
    log_flow() ;

    if (data_type == DATA_SERVICE) {

        if (!graph_build_service(g, base, treename, general))
            return 0 ;

    } else if (data_type == DATA_TREE) {

        if (!graph_build_tree(g, base))
            return 0 ;
    }

    return 1 ;
}
