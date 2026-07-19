/*
 * migrate_0821.c
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

#include <stdlib.h>//free
#include <string.h>//strcmp
#include <sys/stat.h>
#include <unistd.h>

#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/cdb.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/constants.h>
#include <66/module.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/utils.h>

#include <66/migrate_0821.h>
#include <66/migrate.h>

static void migrate_resolve(ssexec_t *info, const char *path, const char *name)
{
    int fd ;
    ocdb c = OCDB_ZERO ;
    resolve_service_t_0821 res = RESOLVE_SERVICE_ZERO_0821 ;

    if (resolve_open_cdb(&fd, &c, path, name) <= 0)
        log_dieusys(LOG_EXIT_SYS, "open resolve file of service: ", name) ;

    if (!service_resolve_read_cdb_0821(&c, &res))
        log_dieusys(LOG_EXIT_SYS, "read resolve file of service: ", name) ;

    if (!migrate_write_frozen_0821(&res, info->base.s, name))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of service: ", name) ;

    strbuf_free(&res.sa) ;
}

static void migrate_service_0821(void)
{
    log_flow() ;

    size_t pos = 0 ;
    char const *exclude[3] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1, 0 } ;
    ssexec_t info = SSEXEC_ZERO ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    info.owner = getuid() ;
    info.ownerlen = uid_format(info.ownerstr, info.owner) ;
    info.ownerstr[info.ownerlen] = 0 ;

    if (!set_ownersysdir(&info.base, info.owner))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    set_info(&info) ;

    char path[info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME + SS_RESOLVE_LEN + 1 + 1] ;
    auto_strings(path, info.base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/") ;
    size_t len = info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 ;

    if (!sbl_dir_get_recursive(&sa, path, exclude, S_IFLNK, 0))
        log_dieu(LOG_EXIT_SYS, "get resolve files") ;

    FOREACH_SBL(&sa, pos) {

        char *name = sa.s + pos ;

        /** A provide alias is a relative symlink to another service; it owns no
         * resolve file of its own and is migrated under its target's real name.
         * Skip it here -- otherwise opening <alias>/.resolve/<alias> fails with
         * ENOENT and aborts the whole migration. */
        char lnk[info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;
        char rname[SS_MAX_SERVICE_NAME + 1] ;
        auto_strings(lnk, info.base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;
        auto_strings(rname, name) ;

        if (!service_resolve_symlink(info.base.s, lnk, rname))
            log_dieusys(LOG_EXIT_SYS, "resolve symlink of service: ", name) ;

        if (strcmp(rname, name))
            continue ;

        auto_strings(path + len, name, SS_RESOLVE, "/") ;

        migrate_resolve(&info, path, name) ;
    }

    ssexec_free(&info) ;
}

void migrate_0821(void)
{
    log_flow() ;

    log_info("Upgrading system from version: 0.8.2.1 to: 0.8.2.2") ;

    migrate_service_0821() ;

    log_info("System successfully upgraded to version: 0.8.2.2") ;
}
