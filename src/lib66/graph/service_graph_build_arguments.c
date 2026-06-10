/*
 * service_graph_build_arguments.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>

#include <66/graph.h>
#include <66/ssexec.h>

uint32_t service_graph_build_arguments(service_graph_t *g, char const *const *argv, int argc, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (!environ_import_arguments(&sa, argv, argc))
        log_dieusys(LOG_EXIT_SYS, "import arguments") ;

    return service_graph_build_list(g, sa.s, sa.len, info, flag) ;
}