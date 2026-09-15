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
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/files.h>

#include <66/config.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/symlink.h>
#include <66/constants.h>

static int symlink_provide_reclaim(char const *base, char const *name, char const *service)
{
    log_flow() ;

    char owner[SS_MAX_SERVICE_NAME + 1] ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    if (!service_resolve_provide(owner, name, base)) {
        resolve_free(wres) ;
        log_warnusys_return(LOG_EXIT_ZERO, "resolve provide alias: ", name) ;
    }

    /** another service already answers to that name */
    if (strcmp(owner, name) && strcmp(owner, service)) {
        resolve_free(wres) ;
        log_warnu_return(LOG_EXIT_ZERO, owner, " is already providing: ", name, " -- disable it first with '66 disable ", name, "' command") ;
    }

    /** a service bearing that name holds it as long as it is enabled */
    if (resolve_read(wres, base, name) > 0 && strcmp(res.sa.s + res.name, service) && res.enabled) {
        resolve_free(wres) ;
        log_warn_return(LOG_EXIT_ZERO, "a service named: ", name, " is already enabled -- disable it first with '66 disable ", name, "' command") ;
    }

    resolve_free(wres) ;
    return 1 ;
}

int symlink_provide(const char *base, resolve_service_t *res, bool action)
{
    log_flow() ;

    if (res->islog || !res->has_dependencies)
        return 1 ;

    char *service = res->sa.s + res->name ;

    /** the provide list lives in the dependencies addon */
    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t_ref wdep = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &dep) ;
    if (resolve_read(wdep, base, service) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read dependencies addon of: ", service) ;

    if (!dep.nprovide) {
        resolve_free(wdep) ;
        return 1 ;
    }

    size_t pos = 0 ;
    _alloc_sbl_(stk, strlen(dep.sa.s + dep.provide)) ;

    if (!sbl_clean_string(&stk, dep.sa.s + dep.provide)) {
        resolve_free(wdep) ;
        log_warnusys_return(LOG_EXIT_ZERO, "clean string") ;
    }

    FOREACH_SBL(&stk, pos) {

        char *name = stk.s + pos ;
        char lnk[strlen(base) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + SS_PROVIDE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;

        auto_strings(lnk, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE, "/", name) ;

        if (action) {

            char target[3 + strlen(service) + 1] ;

            if (!symlink_provide_reclaim(base, name, service)) {
                resolve_free(wdep) ;
                return 0 ;
            }

            /** relative to SS_PROVIDE, so the link resolves to the service entry */
            auto_strings(target, "../", service) ;

            log_trace("symlink: ", lnk, " to: ", target) ;
            if (symlink(target, lnk) < 0 && errno != EEXIST) {
                resolve_free(wdep) ;
                log_warnusys_return(LOG_EXIT_ZERO, "make symlink: ", lnk, " to: ", target) ;
            }

        } else {

            char owner[SS_MAX_SERVICE_NAME + 1] ;

            if (!service_resolve_provide(owner, name, base)) {
                resolve_free(wdep) ;
                log_warnusys_return(LOG_EXIT_ZERO, "resolve provide alias: ", name) ;
            }

            /** the entry belongs to whoever created it */
            if (!strcmp(owner, service)) {
                log_trace("remove provide symlink: ", lnk) ;
                file_tryunlink(lnk) ;
            }
        }
    }

    resolve_free(wdep) ;
    return 1 ;
}
