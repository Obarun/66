/*
 * resolve_write.c
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/constants.h>

static int core_set_has(char const *base, char const *name, uint8_t type)
{
    resolve_service_t core = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &core) ;

    if (resolve_read(wres, base, name) <= 0) {
        resolve_free(wres) ;
        return 0 ;
    }

    uint32_t *has = 0 ;
    switch (type) {

        case DATA_SERVICE_LIMIT: has = &core.has_limit ; break ;

        default: break ;
    }

    int r = 1 ;
    if (has && !*has) {
        *has = 1 ;
        r = resolve_write(wres, base, name) ;
    }

    resolve_free(wres) ;

    return r ;
}

int resolve_write(resolve_wrapper_t *wres, char const *base, char const *name)
{
    log_flow() ;

    size_t baselen = strlen(base) ;
    size_t namelen = strlen(name) ;

    char path[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + 1] ;
    char aname[namelen + SS_ADDON_LIMIT_SUFFIX_LEN + 1] ;

    auto_strings(aname, name) ;

    if (wres->type == DATA_SERVICE) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    } else if (wres->type == DATA_SERVICE_LIMIT) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;
        auto_strings(aname, name, SS_ADDON_LIMIT_SUFFIX) ;

        if (!core_set_has(base, name, wres->type))
            return (errno = EINVAL, 0) ;

    } else if (wres->type == DATA_TREE || wres->type == DATA_TREE_MASTER) {

        auto_strings(path, base, SS_SYSTEM) ;

    } else return 0 ;

    return resolve_write_at(wres, path, aname) ;
}
