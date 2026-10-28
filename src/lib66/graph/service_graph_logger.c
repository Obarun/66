/*
 * service_graph_logger.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>

#include <oblibs/log.h>

#include <66/graph.h>

uint32_t service_graph_logger(service_graph_t const *g, uint32_t index)
{
    log_flow() ;

    uint32_t const *depends = service_graph_depends(g, index) ;
    uint8_t const *origin = service_graph_dorigin(g, index) ;

    for (uint32_t pos = 0 ; pos < service_graph_vertex(g, index)->ndepends ; pos++)
        if (origin[pos] & SERVICE_GRAPH_EDGE_LOGGER)
            return depends[pos] ;

    return SERVICE_GRAPH_NOINDEX ;
}
