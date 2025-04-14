/*
 * tree_graph_build_name.c
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

#include <stdint.h>
#include <errno.h>

#include <oblibs/log.h>

#include <66/graph.h>
#include <66/ssexec.h>

uint32_t tree_graph_build_name(tree_graph_t *g, const char *name, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    uint32_t n = 0 ;

    n = tree_graph_collect(g, name, info) ;
    if (!n)
        return n ;

    if (!tree_graph_resolve(g, name, flag))
        return (errno = EINVAL, 0) ;

    return n ;
}