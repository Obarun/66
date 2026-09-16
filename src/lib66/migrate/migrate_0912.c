/*
 * migrate_0912.c
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
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/migrate.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/ssexec.h>
#include <66/utils.h>

void migrate_0912_provide_symlink(void)
{
    log_flow() ;

    ssexec_t info = SSEXEC_ZERO ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    size_t pos = 0 ;
    char const *exclude[2] = { SS_PROVIDE + 1, 0 } ;

    info.owner = getuid() ;
    info.ownerlen = uid_format(info.ownerstr, info.owner) ;
    info.ownerstr[info.ownerlen] = 0 ;

    if (!set_ownersysdir(&info.base, info.owner))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    set_info(&info) ;

    char dir[info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + SS_PROVIDE_LEN + 1] ;

    auto_strings(dir, info.base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;

    if (!sbl_dir_get_recursive(&sa, dir, exclude, S_IFLNK, 0))
        log_dieusys(LOG_EXIT_SYS, "get resolve files") ;

    auto_strings(dir + info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN, SS_PROVIDE) ;

    if (!dir_create_parent(dir, 0755))
        log_dieusys(LOG_EXIT_SYS, "create directory: ", dir) ;

    FOREACH_SBL(&sa, pos) {

        char *name = sa.s + pos ;
        char l[SS_MAX_PATH_LEN + 1] ;
        char old[info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;
        char new[info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + SS_PROVIDE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;
        char target[3 + SS_MAX_SERVICE_NAME + 1] ;

        auto_strings(old, info.base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        ssize_t len = readlink(old, l, SS_MAX_PATH_LEN) ;
        if (len < 1)
            log_dieusys(LOG_EXIT_SYS, "readlink: ", old) ;

        l[len] = 0 ;

        /** an absolute target is a service's own entry, it stays where it is */
        if (l[0] == '/')
            continue ;

        auto_strings(new, info.base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE, "/", name) ;
        auto_strings(target, "../", l) ;

        log_trace("move provide alias: ", old, " to: ", new) ;

        if (symlink(target, new) < 0)
            log_dieusys(LOG_EXIT_SYS, "point symlink: ", new, " to: ", target) ;

        file_tryunlink(old) ;
    }

    {
        _cleanup_strbuf_ strbuf names = STRBUF_ZERO ;
        size_t npos = 0 ;

        if (!sbl_dir_get_recursive(&names, dir, exclude, S_IFLNK, 0))
            log_dieusys(LOG_EXIT_SYS, "get provided names") ;

        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

        FOREACH_SBL(&names, npos) {

            char *name = names.s + npos ;
            char lnk[info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + SS_PROVIDE_LEN + 1 + strlen(name) + 1] ;

            auto_strings(lnk, info.base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE, "/", name) ;

            if (!resolve_check(wres, info.base.s, name))
                continue ;

            log_warn("service: ", name, " has a name a provider claims -- the service keeps it, remove one of them to hand it over") ;
            file_tryunlink(lnk) ;
        }

        free(wres) ;
    }
}
