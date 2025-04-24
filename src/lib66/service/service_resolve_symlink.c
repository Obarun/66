/*
 * service_resolve_symlink.h
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>
#include <unistd.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/symlink.h>

int service_resolve_symlink(char const *base, char *path, char *name)
{
    log_flow() ;
    char l[SS_MAX_PATH_LEN + 1] ;
    char ln[SS_MAX_SERVICE_NAME + 1] ;

    ssize_t len = readlink(path, l, SS_MAX_PATH_LEN) ;

    if (len < 0)
        return 1 ;

    if (len >= SS_MAX_PATH_LEN)
        return (errno = EINVAL, 0) ;

    l[len] = 0 ;

    if (l[0] != '/') {
        auto_strings(ln, l) ;
        auto_strings(l, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", ln) ;
    }

    if (len > 0 && symlink_type(l) > 0) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", ln) ;

        auto_strings(name, ln) ;
    }

    return 1 ;
}