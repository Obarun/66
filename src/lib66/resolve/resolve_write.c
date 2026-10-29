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
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/constants.h>
#include <66/graph.h>
#include <66/utils.h>

static int core_set_has(char const *base, char const *name, uint8_t type)
{
    resolve_service_t core = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE, &core) ;

    if (resolve_read(&wres, base, name) <= 0) {
        resolve_free_obj(&wres) ;
        return 0 ;
    }

    uint32_t *has = 0 ;
    switch (type) {

        case DATA_SERVICE_LIMIT: has = &core.has_limit ; break ;
        case DATA_SERVICE_ENVIRON: has = &core.has_environ ; break ;
        case DATA_SERVICE_IO: has = &core.has_io ; break ;
        case DATA_SERVICE_EXECUTE: has = &core.has_execute ; break ;
        case DATA_SERVICE_DEPENDENCIES: has = &core.has_dependencies ; break ;
        case DATA_SERVICE_REGEX: has = &core.has_regex ; break ;
        case DATA_SERVICE_EVENT: has = &core.has_event ; break ;

        default: break ;
    }

    int r = 1 ;
    if (has && !*has) {
        *has = 1 ;
        r = resolve_write(&wres, base, name) ;
    }

    resolve_free_obj(&wres) ;

    return r ;
}

int resolve_write(resolve_wrapper_t *wres, char const *base, char const *name)
{
    log_flow() ;

    char const *suffix = resolve_addon_suffix(wres->type) ;

    if (!suffix)
        return (errno = EINVAL, 0) ;

    size_t baselen = strlen(base) ;
    char path[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + strlen(name) + 1] ;
    char tmpdir[TMPDIR_GETLEN(baselen) + 1] ;

    if (wres->type == DATA_TREE || wres->type == DATA_TREE_MASTER) {

        auto_strings(path, base, SS_SYSTEM) ;

    } else {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        // an addon is declared by the has_ field of its resolve file
        if (wres->type != DATA_SERVICE && !core_set_has(base, name, wres->type))
            return 0 ;
    }

    // the cdb is prepared in <base>/.tmp, then renamed onto <path>/.resolve/<name><suffix>
    auto_strings(tmpdir, base, SS_TMPDIR + 1) ;

    return resolve_write_at(wres, path, name, tmpdir) ;
}
