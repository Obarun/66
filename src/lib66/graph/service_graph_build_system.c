/*
 * service_graph_build_system.c
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
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/constants.h>

uint32_t service_graph_build_system(service_graph_t *g, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    _alloc_sa_(sa) ;
    char const *exclude[1] = { 0 } ;
    char solve[info->base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1] ;

    auto_strings(solve, info->base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;

    if (!sastr_dir_get_recursive(&sa, solve, exclude, S_IFLNK, 0))
        log_warnu_return(LOG_EXIT_ZERO, "get resolve files") ;

    return service_graph_build_list(g, sa.s, sa.len, info, flag) ;
}
