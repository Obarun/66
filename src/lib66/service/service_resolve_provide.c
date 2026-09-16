/*
 * service_resolve_provide.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/service.h>

int service_resolve_provide(char *dst, char const *src, char const *base)
{
    log_flow() ;

    char path[SS_MAX_PATH_LEN] ;
    char lnk[SS_MAX_PATH_LEN + 1] ;

    auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE, "/", src) ;

    ssize_t len = readlink(path, lnk, SS_MAX_PATH_LEN) ;

    if (len < 0) {

        // any other failure leaves the answer unknown
        if (errno != ENOENT)
            return 0 ;

        // no entry: the name is not provided, it answers for itself
        auto_strings(dst, src) ;
        return 1 ;
    }

    lnk[len] = 0 ;

    // the link is written as ../<provider>, so that it resolves on disk
    char *provider = !strncmp(lnk, "../", 3) ? lnk + 3 : lnk ;

    if (!*provider)
        return (errno = EINVAL, 0) ;

    auto_strings(dst, provider) ;

    return 1 ;
}
