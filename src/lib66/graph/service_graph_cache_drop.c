/*
 * service_graph_cache_drop.c
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

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/fd.h>
#include <oblibs/string.h>

#include <66/constants.h>

#include <66/graph.h>

int service_graph_cache_drop(uid_t owner, char const *live)
{
    log_flow() ;

    char dir[SERVICE_GRAPH_CACHE_DIR_LEN(strlen(live)) + 1 + SS_LIVEGRAPH_FILE_LEN + 1] ;
    int fd ;

    SERVICE_GRAPH_CACHE_DIR(dir, live, owner) ;

    fd = service_graph_cache_dir(dir, owner) ;
    if (fd < 0)
        return (errno == ENOENT || errno == EPERM) ? 1 : -1 ;

    close_fd(fd) ;

    auto_strings(dir + strlen(dir), "/", SS_LIVEGRAPH_FILE) ;

    log_trace("drop the cache of the graph: ", dir) ;

    if (unlink(dir) < 0 && errno != ENOENT)
        return -1 ;

    return 1 ;
}
