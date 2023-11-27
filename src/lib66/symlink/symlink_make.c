/*
 * symlink_make.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/service.h>
#include <66/constants.h>

int symlink_make(resolve_service_t *res)
{
    log_flow() ;

    int r ;
    struct stat st ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name), homelen = strlen(res->sa.s + res->path.home) ;
    char *dst = res->sa.s + res->path.servicedir ;
    char sym[homelen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + 1] ;

    auto_strings(sym, res->sa.s + res->path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    r = lstat(sym, &st) ;
    if (r < 0) {

        if (errno != ENOENT) {

            log_warnusys_return(LOG_EXIT_ZERO, "lstat: ", sym) ;

        } else {

            log_trace("symlink: ", sym, " to: ", dst) ;
            r = symlink(dst, sym) ;
            if (r < 0 && errno != EEXIST)
                log_warnusys_return(LOG_EXIT_ZERO, "point symlink: ", sym, " to: ", dst) ;
        }
    }

    return 1 ;
}