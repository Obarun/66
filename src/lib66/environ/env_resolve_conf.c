/*
 * env_resolve_conf.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/environ.h>
#include <66/constants.h>
#include <66/utils.h>
#include <66/service.h>

int env_resolve_conf(stralloc *env, resolve_service_t *res)
{
    log_flow() ;

    if (!res->owner) {

        if (!stralloc_cats(env, SS_SERVICE_ADMCONFDIR))
            return 0 ;

    } else {

        if (!set_ownerhome(env, res->owner))
            return 0 ;

        if (!stralloc_cats(env, SS_SERVICE_USERCONFDIR))
            return 0 ;
    }

    if (res->inns)
        if (!auto_stra(env, res->sa.s + res->inns, SS_SYM_VERSION, "/"))
            return 0 ;

    if (!auto_stra(env, res->sa.s + res->name))
        return 0 ;

    return 1 ;
}
