/*
 * graph_build_arguments.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <oblibs/environ.h>

#include <66/graph.h>
#include <66/hash.h>
#include <66/ssexec.h>


void graph_build_arguments(graph_t *graph, char const *const *argv, int argc, struct resolve_hash_s **hres, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    _alloc_sa_(sa) ;

    if (!environ_import_arguments(&sa, argv, argc))
        log_dieusys(LOG_EXIT_SYS, "import arguments") ;

    service_graph_g(sa.s, sa.len, graph, hres, info, flag) ;
}