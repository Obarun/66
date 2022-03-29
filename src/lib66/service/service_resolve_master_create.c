/*
 * service_resolve_master_create.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/service.h>

int service_resolve_master_create(char const *base, char const *treename)
{

    log_flow() ;

    int e = 0 ;
    size_t baselen = strlen(base), treelen = strlen(treename) ;
    resolve_service_master_t mres = RESOLVE_SERVICE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_MASTER, &mres) ;
    char dst[baselen + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN + 1] ;

    resolve_init(wres) ;

    mres.name = resolve_add_string(wres, SS_MASTER + 1) ;

    auto_strings(dst, base, SS_SYSTEM, "/", treename, SS_SVDIRS) ;

    log_trace("write Master resolve file of services") ;
    if (!resolve_write(wres, dst, SS_MASTER + 1))
        goto err ;

    e = 1 ;

    err:
        resolve_free(wres) ;
        return e ;
}
