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
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/resolve.h>
#include <66/constants.h>
#include <66/service.h>

int resolve_check(resolve_wrapper_t *wres, char const *base, char const *name)
{
    log_flow() ;

    char path[SS_MAX_PATH_LEN] ;
    char lname[SS_MAX_SERVICE_NAME + SS_ADDON_DEPENDENCIES_SUFFIX_LEN + 1] ; // longest addon suffix

    auto_strings(lname, name) ;

    if (wres->type == DATA_SERVICE) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    } else if (wres->type == DATA_SERVICE_LIMIT) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        auto_strings(lname + strlen(lname), SS_ADDON_LIMIT_SUFFIX) ;

    } else if (wres->type == DATA_SERVICE_ENVIRON) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        auto_strings(lname + strlen(lname), SS_ADDON_ENVIRON_SUFFIX) ;

    } else if (wres->type == DATA_SERVICE_IO) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        auto_strings(lname + strlen(lname), SS_ADDON_IO_SUFFIX) ;

    } else if (wres->type == DATA_SERVICE_EXECUTE) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        auto_strings(lname + strlen(lname), SS_ADDON_EXECUTE_SUFFIX) ;

    } else if (wres->type == DATA_SERVICE_DEPENDENCIES) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        auto_strings(lname + strlen(lname), SS_ADDON_DEPENDENCIES_SUFFIX) ;

    } else if (wres->type == DATA_SERVICE_REGEX) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        auto_strings(lname + strlen(lname), SS_ADDON_REGEX_SUFFIX) ;

    } else if (wres->type == DATA_SERVICE_EVENT) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        auto_strings(lname + strlen(lname), SS_ADDON_EVENT_SUFFIX) ;

    } else if (wres->type == DATA_TREE || wres->type == DATA_TREE_MASTER) {

        auto_strings(path, base, SS_SYSTEM) ;

    } else return 0 ;

    return resolve_check_at(path, lname) ;

}
