/*
 * service_isenabled.c
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

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>

int service_isenabled(char const *base, char const *name)
{
    log_flow() ;

    int e = -1 ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    int r = resolve_read(wres, base, name) ;

    if (r > 0)
        e = res.enabled ? 1 : 0 ;
    else if (!r)
        e = 0 ;

    resolve_free(wres) ;
    return e ;
}
