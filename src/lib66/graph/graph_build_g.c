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


#include <oblibs/log.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/graph.h>
#include <66/ssexec.h>

void graph_build_g(graph_t *graph, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info)
{
    log_flow() ;

    if (data_type == DATA_SERVICE)

        graph_build_service(graph, ares, areslen, info) ;

    else if (data_type == DATA_TREE)

        graph_build_tree(g, info->base.s) ;
}
