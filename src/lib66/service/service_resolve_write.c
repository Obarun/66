/*
 * service_resolve_write.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/constants.h>

int service_resolve_write(resolve_service_t *res)
{
    log_flow() ;

    int r, e = 0 ;

    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;
    char sym[strlen(res->sa.s + res->path.home) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + 1] ;
    char dst[strlen(res->sa.s + res->path.tree) + SS_SVDIRS_LEN + 1] ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    auto_strings(sym, res->sa.s + res->path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    auto_strings(dst, res->sa.s + res->path.tree, SS_SVDIRS) ;

    if (!resolve_write(wres, dst, name))
        goto err ;

    r = symlink(dst, sym) ;
    if (r < 0 && errno != EEXIST) {
        log_warnusys("point symlink: ", sym, " to: ", dst) ;
        goto err ;
    }

    e = 1 ;

    err:
        free(wres) ;
        return e ;
}


