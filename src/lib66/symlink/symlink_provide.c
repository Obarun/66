/*
 * symlink_provide.c
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

#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/files.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/symlink.h>
#include <66/constants.h>

int symlink_provide(const char *base, resolve_service_t *res, bool action)
{
    log_flow() ;

    if (res->islog || !res->has_dependencies)
        return 1 ;

    /** the provide list lives in the dependencies addon */
    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t_ref wdep = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &dep) ;
    if (resolve_read(wdep, base, res->sa.s + res->name) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read dependencies addon of: ", res->sa.s + res->name) ;
    free(wdep) ;

    if (!dep.nprovide) {
        strbuf_free(&dep.sa) ;
        return 1 ;
    }

    size_t pos = 0 ;
    char path[SS_MAX_PATH_LEN] ;
    _alloc_sbl_(stk, strlen(dep.sa.s + dep.provide)) ;
    char lnk[strlen(base) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME] ;
    char lname[SS_MAX_PATH_LEN] ;

    if (!sbl_clean_string(&stk, dep.sa.s + dep.provide)) {
        strbuf_free(&dep.sa) ;
        log_warnusys_return(LOG_EXIT_ZERO, "clean string") ;
    }

    FOREACH_SBL(&stk, pos) {

        char *name = stk.s + pos ;

        auto_strings(lnk, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        if (symlink_type(lnk) > 0) {

            auto_strings(lname, name) ;

            if (!service_resolve_symlink(base, path, lname)) {
                log_warnusys("resolve symlink path: ", lnk) ;
                continue ;
            }

            if (action) {

                if (strcmp(lname, res->sa.s + res->name))
                    log_1_warn_return(LOG_EXIT_ZERO, lname, " is already providing: ", name, " -- disable it first with '66 disable ", name, "' command") ;

            } else if (!strcmp(lname, res->sa.s + res->name)) {

                log_trace("remove provide symlink: ", lnk) ;
                file_tryunlink(lnk) ;
            }

        } else if (action) {

            log_trace("symlink: ", path, " to: ", res->sa.s + res->name) ;
            int r = symlink(res->sa.s + res->name, path) ;
            if (r < 0 && errno != EEXIST) {
                strbuf_free(&dep.sa) ;
                log_warnusys_return(LOG_EXIT_ZERO, "make symlink: ", path, " to: ", res->sa.s + res->name) ;
            }
        }
    }

    strbuf_free(&dep.sa) ;
    return 1 ;
}