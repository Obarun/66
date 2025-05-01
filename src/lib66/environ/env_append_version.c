/*
 * env_appand_version.c
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

#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>

#include <66/environ.h>

int env_append_version(stralloc *saversion, char const *svconf, char const *version)
{
    log_flow() ;

    int r ;

    _alloc_stk_(stk, strlen(version) + 1) ;

    auto_strings(stk.s, version) ;

    if (!auto_stra(saversion,svconf,"/",stk.s))
        log_warnusys_return(LOG_EXIT_ZERO,"stralloc") ;

    r = scan_mode(saversion->s,S_IFDIR) ;
    if (r == -1 || !r)
        log_warnusys_return(LOG_EXIT_ZERO,"find the versioned directory: ",saversion->s) ;

    return 1 ;
}
