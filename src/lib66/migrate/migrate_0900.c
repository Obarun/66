/*
 * migrate_0900.c
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

#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/migrate.h>
#include <66/module.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/ssexec.h>
#include <66/utils.h>

/* 0.9.0.0 wrote the event addon without eventpropagate. Reading it with the
 * current reader fails on the missing key, and every graph build that touches a
 * service carrying an [Event] section dies on it -- so the addon is rewritten
 * here rather than left for the first command to trip over. */
static int read_addon_event_0900(ocdb *c, resolve_service_addon_event_t *ev)
{
    log_flow() ;

    if (resolve_get_sa(&ev->sa, c) <= 0 || !ev->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key_u32(c, "rversion", &ev->rversion) ||
        !resolve_get_key_u32(c, "eventtype", &ev->type) ||
        !resolve_get_key_u32(c, "eventfrom", &ev->from) ||
        !resolve_get_key_u32(c, "neventfrom", &ev->nfrom) ||
        !resolve_get_key_u32(c, "eventon", &ev->on) ||
        !resolve_get_key_u32(c, "neventon", &ev->non) ||
        !resolve_get_key_u32(c, "eventcombine", &ev->combine) ||
        !resolve_get_key_u32(c, "eventdo", &ev->docmd) ||
        !resolve_get_key_u32(c, "eventemit", &ev->emit) ||
        !resolve_get_key_u32(c, "eventwatch", &ev->watch) ||
        !resolve_get_key_u32(c, "eventexpression", &ev->expression) ||
        !resolve_get_key_u32(c, "eventtimezone", &ev->timezone) ||
        !resolve_get_key_u32(c, "eventinterval", &ev->interval))
            return (errno = EINVAL, 0) ;

    return 1 ;
}

static void migrate_event_0900(ssexec_t *info, char const *path, char const *name)
{
    log_flow() ;

    int fd ;
    ocdb c = OCDB_ZERO ;
    resolve_service_addon_event_t ev = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;

    char aname[strlen(name) + SS_ADDON_EVENT_SUFFIX_LEN + 1] ;
    auto_strings(aname, name, SS_ADDON_EVENT_SUFFIX) ;

    // a service without an [Event] section owns no such addon
    if (resolve_open_cdb(&fd, &c, path, aname) <= 0)
        return ;

    if (!read_addon_event_0900(&c, &ev))
        log_dieusys(LOG_EXIT_SYS, "read event addon of service: ", name) ;

    ev.propagate = 1 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EVENT, &ev) ;

    if (!resolve_write(wres, info->base.s, name)) {
        resolve_free(wres) ;
        log_dieusys(LOG_EXIT_SYS, "write event addon of service: ", name) ;
    }

    log_trace("event addon migrated for: ", name) ;

    resolve_free(wres) ;
}

void migrate_0900(void)
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

        /* a provide alias is a relative symlink to another service and owns no
         * resolve of its own: it is migrated under its target's real name. */
        char lnk[info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;
        char rname[SS_MAX_SERVICE_NAME + 1] ;
        auto_strings(lnk, info.base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;
        auto_strings(rname, name) ;

        if (!service_resolve_symlink(info.base.s, lnk, rname))
            log_dieusys(LOG_EXIT_SYS, "resolve symlink of service: ", name) ;

        if (strcmp(rname, name))
            continue ;

        auto_strings(path + len, name, SS_RESOLVE, "/") ;

        migrate_event_0900(&info, path, name) ;
    }

    ssexec_free(&info) ;
}
