/*
 * resolve_write_g.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

int resolve_write_g(resolve_wrapper_t *wres, char const *base, char const *name)
{
    log_flow() ;

    size_t baselen = strlen(base) ;
    size_t namelen = strlen(name) ;

    char path[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + 1] ;

    if (wres->type == DATA_SERVICE) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    } else if (wres->type == DATA_TREE || wres->type == DATA_TREE_MASTER) {

        auto_strings(path, base, SS_SYSTEM) ;

    } else return 0 ;

    return resolve_write(wres, path, name) ;
}
