/*
 * env_check_version.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/environ.h>
#include <66/constants.h>

int env_check_version(stralloc *sa, char const *version)
{
    log_flow() ;

    int r ;

    r = version_scan(sa,version,SS_CONFIG_VERSION_NDOT) ;

    if (r == -1)
        log_warnusys_return(LOG_EXIT_ZERO,"stralloc") ;

    if (!r)
        log_warn_return(LOG_EXIT_ZERO,"invalid version format: ",version) ;

    return 1 ;
}
