/*
 * resolve_check.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/resolve.h>
#include <66/constants.h>
#include <66/service.h>

int resolve_check(resolve_wrapper_t *wres, char const *base, char const *name)
{
    log_flow() ;

    char const *suffix = resolve_addon_suffix(wres->type) ;

    if (!suffix)
        return (errno = EINVAL, -1) ;

    char path[SS_MAX_PATH_LEN] ;
    char lname[strlen(name) + strlen(suffix) + 1] ;

    if (wres->type == DATA_TREE || wres->type == DATA_TREE_MASTER)
        auto_strings(path, base, SS_SYSTEM) ;
    else
        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    auto_strings(lname, name, suffix) ;

    return resolve_check_at(path, lname) ;
}
