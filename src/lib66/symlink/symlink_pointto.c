/*
 * symkink_pointto.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/symlink.h>
#include <66/resolve.h>
#include <66/config.h>
#include <66/constants.h>

/**
 * Return SYMLINK_SOURCE is symlink point to source
 * Return SYMLINK_LIVE is symlink point to live
 * Return -1 on error
 **/
int symlink_pointto(resolve_service_t *res)
{
    log_flow() ;

    size_t homelen = strlen(res->sa.s + res->path.home), namelen = strlen(res->sa.s + res->name) ;
    char sym[homelen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + 1] ;
    char realsym[SS_MAX_PATH_LEN + 1] ;
    int size = 0 ;

    auto_strings(sym, res->sa.s + res->path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", res->sa.s + res->name) ;

    size = readlink(sym, realsym, SS_MAX_PATH_LEN) ;
    if (size >= SS_MAX_PATH_LEN) {
        errno = ENAMETOOLONG ;
        log_warnusys_return(LOG_EXIT_LESSONE, "readlink") ;
    }

    realsym[size] = 0 ;

    if (str_start_with(realsym, res->sa.s + res->path.home))
        return SYMLINK_SOURCE ;
    else
        return SYMLINK_LIVE ;
}