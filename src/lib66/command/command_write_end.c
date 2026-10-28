/*
 * command_write_end.c
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

#include <oblibs/log.h>

#include <66/graph.h>

void command_write_end(service_graph_t *g)
{
    log_flow() ;

    int r = service_graph_write_end(g) ;

    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, g ? "load the graph of the system" : "release the lock of the system") ;

    if (!r)
        log_warnusys("write the cache of the graph of the system") ;
}
