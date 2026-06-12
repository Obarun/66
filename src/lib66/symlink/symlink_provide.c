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

#include <66/service.h>
#include <66/symlink.h>
#include <66/constants.h>

int symlink_provide(const char *base, resolve_service_t *res, bool action)
{
    log_flow() ;

    if (res->islog)
        return 1 ;

    size_t pos = 0 ;
    _alloc_strbuf_(path, SS_MAX_PATH_LEN) ;
    _alloc_sbl_(stk, strlen(res->sa.s + res->dependencies.provide)) ;
    _alloc_strbuf_(lnk, strlen(base) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME) ;
    _alloc_strbuf_(lname, SS_MAX_PATH_LEN) ;

    if (!sbl_clean_string(&stk, res->sa.s + res->dependencies.provide))
        log_warnusys_return(LOG_EXIT_ZERO, "clean string") ;

    FOREACH_SBL(&stk, pos) {

        char *name = stk.s + pos ;

        auto_strings(lnk.s, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        auto_strings(path.s, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        if (symlink_type(lnk.s) > 0) {

            auto_strings(lname.s, name) ;

            if (!service_resolve_symlink(base, path.s, lname.s)) {
                log_warnusys("resolve symlink path: ", lnk.s) ;
                continue ;
            }

            if (action) {

                if (strcmp(lname.s, res->sa.s + res->name))
                    log_1_warn_return(LOG_EXIT_ZERO, lname.s, " is already providing: ", name, " -- disable it first with '66 disable ", name, "' command") ;

            } else if (!strcmp(lname.s, res->sa.s + res->name)) {

                log_trace("remove provide symlink: ", lnk.s) ;
                unlink(lnk.s) ;
            }

        } else if (action) {

            log_trace("symlink: ", path.s, " to: ", res->sa.s + res->name) ;
            int r = symlink(res->sa.s + res->name, path.s) ;
            if (r < 0 && errno != EEXIST)
                log_warnusys_return(LOG_EXIT_ZERO, "make symlink: ", path.s, " to: ", res->sa.s + res->name) ;
        }
    }

    return 1 ;
}