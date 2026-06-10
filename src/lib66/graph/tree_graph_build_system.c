/*
 * tree_graph_build_system.c
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
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/constants.h>

uint32_t tree_graph_build_system(tree_graph_t *g, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    char const *exclude[2] = { SS_MASTER + 1, 0 } ;
    char solve[info->base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + 1] ;

    auto_strings(solve, info->base.s, SS_SYSTEM, SS_RESOLVE) ;

    if (!sbl_dir_get(&sa, solve, exclude, S_IFREG))
        log_warnu_return(LOG_EXIT_ZERO, "get resolve files") ;

    return tree_graph_build_list(g, sa.s, sa.len, info, flag) ;
}