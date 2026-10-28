/*
 * service_graph_reload.c
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

#include <sys/types.h>

#include <oblibs/log.h>

#include <66/graph.h>

int service_graph_reload(service_graph_t *g, uid_t owner, char const *base, char const *live)
{
    log_flow() ;

    int r = service_graph_write_try(owner, base, live) ;

    if (r < 0)
        return -1 ;

    if (!r) {
        service_graph_free(g) ;
        return service_graph_build(g, base, 0) < 0 ? -1 : 1 ;
    }

    r = service_graph_write_end(g) ;
    if (r < 0)
        return -1 ;

    if (!r)
        log_warnusys("write the cache of the graph of the system") ;

    return 1 ;
}
