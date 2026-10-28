/*
 * service_graph_load.c
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

int service_graph_load(service_graph_t *g, uid_t owner, char const *base, char const *live)
{
    log_flow() ;

    int r = service_graph_cache_read(g, owner, base, live) ;

    if (r)
        return r ;

    return service_graph_reload(g, owner, base, live) ;
}
