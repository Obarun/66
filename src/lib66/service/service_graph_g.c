/*
 * service_graph_g.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <66/service.h>
#include <66/graph.h>
#include <66/ssexec.h>

void service_graph_g(char const *alist, size_t alen, graph_t *graph, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    service_graph_collect(graph, alist, alen, ares, areslen, info, flag) ;

    if (!*areslen) {
        log_warn("no services matching the requirements at tree: ", info->treename.s) ;
        return ;
    }

    service_graph_build(graph, ares, (*areslen), flag) ;
}
