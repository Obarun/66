/*
 * resolve_read_g.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/resolve.h>
#include <66/constants.h>
#include <66/service.h>

int resolve_read_g(resolve_wrapper_t *wres, char const *base, char const *name)
{
    log_flow() ;

    if (!resolve_check_g(wres, base, name))
        return 0 ;

    char path[SS_MAX_PATH_LEN] ;
    char lname[SS_MAX_PATH_LEN + 1] ;

    auto_strings(lname, name) ;

    if (wres->type == DATA_SERVICE) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        if (!service_resolve_symlink(base, path, lname))
            log_warnusys_return(LOG_EXIT_ZERO, "resolve symlink path") ;

    } else if (wres->type == DATA_TREE || wres->type == DATA_TREE_MASTER) {

        auto_strings(path, base, SS_SYSTEM) ;
    }

    return resolve_read(wres, path, lname) ;
}
