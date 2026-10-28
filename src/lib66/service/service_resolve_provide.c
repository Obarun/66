/*
 * service_resolve_provide.c
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/hash.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/service.h>
#include <66/utils.h>

int service_resolve_provide_along(struct resolve_hash_s **holder, hash_t *along, struct resolve_hash_s const *except, char const *name)
{
    log_flow() ;

    struct resolve_hash_s *c, *tmp ;

    HASH_FOREACH(along, c, tmp) {

        resolve_service_addon_dependencies_t const *dep = &c->dependencies ;

        if (c == except || !dep->nprovide)
            continue ;

        int r = name_inlist(dep->sa.s + dep->provide, name) ;
        if (r < 0)
            return -1 ;

        if (r) {
            *holder = c ;
            return 1 ;
        }
    }

    return 0 ;
}

int service_resolve_provide(char *dst, char const *src, hash_t *overlay, char const *base)
{
    log_flow() ;

    size_t srclen = strlen(src) ;

    if (!srclen || srclen > SS_MAX_SERVICE_NAME)
        return 0 ;

    if (overlay) {

        if (resolve_hash_search(overlay, src))
            return 0 ;

        struct resolve_hash_s *holder ;

        int r = service_resolve_provide_along(&holder, overlay, 0, src) ;
        if (r < 0)
            return -1 ;

        if (r) {
            auto_strings(dst, holder->name) ;
            return 1 ;
        }
    }

    char path[SS_MAX_PATH_LEN] ;
    char lnk[SS_MAX_PATH_LEN + 1] ;

    auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE, "/", src) ;

    ssize_t len = readlink(path, lnk, SS_MAX_PATH_LEN) ;

    if (len < 0) {

        if (errno == ENOENT)
            return 0 ;

        if (errno != EINVAL)
            return -1 ;

        log_warn("damaged Provide entry, ignored: ", path) ;
        return 0 ;
    }

    lnk[len] = 0 ;

    // the link is written as ../<provider>, so that it resolves on disk
    char *provider = !strncmp(lnk, "../", 3) ? lnk + 3 : lnk ;

    if (!*provider) {
        log_warn("damaged Provide entry, ignored: ", path) ;
        return 0 ;
    }

    if (!strcmp(provider, src))
        return 0 ;

    auto_strings(dst, provider) ;

    return 1 ;
}
