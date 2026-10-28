/*
 * service_graph_frontend_gone.c
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
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/graph.h>
#include <66/service.h>
#include <66/module.h>

int service_graph_frontend_gone(service_graph_t const *g, uint32_t index, strbuf *frontends)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    char const *exclude[2] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1 } ;

    // a logger and a member of a module come from the frontend of another service
    if (service_graph_logparent(g, index) != SERVICE_GRAPH_NOINDEX || service_graph_module(g, index) != SERVICE_GRAPH_NOINDEX)
        return 0 ;

    int r = service_frontend_walk(&sa, service_graph_name(g, index), frontends, exclude, 2) ;
    if (r < 0)
        return -1 ;

    return !r ;
}
