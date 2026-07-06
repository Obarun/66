/*
 * resolve_read.c
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

static int resolve_path(char *path, char *lname, char const *base, char const *name)
{
    auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    if (!service_resolve_symlink(base, path, lname))
        log_warnusys_return(LOG_EXIT_ZERO, "resolve symlink path") ;

    return 1 ;
}

int resolve_read(resolve_wrapper_t *wres, char const *base, char const *name)
{
    log_flow() ;

    if (!resolve_check(wres, base, name))
        return 0 ;

    char path[SS_MAX_PATH_LEN] ;
    char lname[SS_MAX_PATH_LEN + 1] ;

    auto_strings(lname, name) ;

    if (wres->type == DATA_SERVICE) {

        if (!resolve_path(path, lname, base, name))
            return 0 ;

    } else if (wres->type == DATA_SERVICE_LIMIT) {

        if (!resolve_path(path, lname, base, name))
            return 0 ;

        auto_strings(lname + strlen(lname), SS_ADDON_LIMIT_SUFFIX) ;

    } else if (wres->type == DATA_SERVICE_ENVIRON) {

        if (!resolve_path(path, lname, base, name))
            return 0 ;

        auto_strings(lname + strlen(lname), SS_ADDON_ENVIRON_SUFFIX) ;

    } else if (wres->type == DATA_SERVICE_IO) {

        if (!resolve_path(path, lname, base, name))
            return 0 ;

        auto_strings(lname + strlen(lname), SS_ADDON_IO_SUFFIX) ;

    } else if (wres->type == DATA_SERVICE_LOGGER) {

        if (!resolve_path(path, lname, base, name))
            return 0 ;

        auto_strings(lname + strlen(lname), SS_ADDON_LOGGER_SUFFIX) ;

    } else if (wres->type == DATA_TREE || wres->type == DATA_TREE_MASTER) {

        auto_strings(path, base, SS_SYSTEM) ;
    }

    return resolve_read_at(wres, path, lname) ;
}
