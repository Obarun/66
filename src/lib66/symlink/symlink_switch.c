/*
 * symlink_switch.c
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

#include <stdint.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/unix-transactional.h>//atomic_symlink

#include <66/resolve.h>
#include <66/sanitize.h>
#include <66/constants.h>


int symlink_switch(resolve_service_t *res, uint8_t flag)
{
    log_flow() ;

    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(res->sa.s + res->name), homelen = strlen(res->sa.s + res->path.home) ;
    char sym[homelen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + 1] ;
    char dst[SS_MAX_PATH_LEN + 1] ;

    auto_strings(sym, res->sa.s + res->path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    if (!flag) {

        auto_strings(dst, res->sa.s + res->path.home, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;

    } else {

        auto_strings(dst, res->sa.s + res->live.servicedir) ;
    }

    log_trace("switch symlink: ", sym, " to: ", dst) ;
    if (!atomic_symlink(dst, sym, "symlink_switch"))
        log_warnusys_return(LOG_EXIT_ZERO, "point symlink: ", sym, " to: ", dst) ;

    return 1 ;
}
