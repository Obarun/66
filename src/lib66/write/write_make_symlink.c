/*
 * write_make_symlink.c
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

#include <string.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/service.h>
#include <66/constants.h>

void write_make_symlink(resolve_service_t *res)
{
    log_flow() ;

    int r ;
    struct stat st ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name), homelen = strlen(res->sa.s + res->path.home) ;

    char sym[homelen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + 1] ;
    char dst[homelen + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + SS_RESOLVE_LEN + 1 + namelen + 1] ;

    auto_strings(sym, res->sa.s + res->path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    auto_strings(dst, res->sa.s + res->path.home, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;

    r = lstat(sym, &st) ;
    if (r < 0) {

        if (errno != ENOENT) {

            log_dieusys(LOG_EXIT_SYS, "lstat: ", sym) ;

        } else {

            log_trace("symlink: ", sym, " to: ", dst) ;
            r = symlink(dst, sym) ;
            if (r < 0 && errno != EEXIST)
                log_dieusys(LOG_EXIT_SYS, "point symlink: ", sym, " to: ", dst) ;
        }
    }
}