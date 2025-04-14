/*
 * service_graph_build_list.c
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

#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#include <oblibs/log.h>

#include <66/ssexec.h>
#include <66/graph.h>

uint32_t service_graph_build_list(service_graph_t *g, const char *list, size_t len, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    uint32_t n = 0 ;

    n = service_graph_ncollect(g, list, len, info, flag) ;
    if (!n)
        return n ;

    if (!service_graph_nresolve(g, list, len, flag))
        return (errno = EINVAL, 0) ;

    return n ;
}
