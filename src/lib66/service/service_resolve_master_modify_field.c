/*
 * service_resolve_master_modify_field.c
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

#include <stdint.h>
#include <stdlib.h>

#include <oblibs/log.h>

#include <skalibs/types.h>

#include <66/tree.h>
#include <66/resolve.h>

#include <66/service.h>

resolve_field_table_t resolve_service_master_field_table[] = {

    [SERVICE_ENUM_MASTER_NAME] = { .field = "name" },
    [SERVICE_ENUM_MASTER_CLASSIC] = { .field = "classic" },
    [SERVICE_ENUM_MASTER_BUNDLE] = { .field = "bundle" },
    [SERVICE_ENUM_MASTER_LONGRUN] = { .field = "longrun" },
    [SERVICE_ENUM_MASTER_ONESHOT] = { .field = "oneshot" },
    [SERVICE_ENUM_MASTER_MODULE] = { .field = "module" },

    [SERVICE_ENUM_MASTER_NCLASSIC] = { .field = "nclassic" },
    [SERVICE_ENUM_MASTER_NBUNDLE] = { .field = "nbundle" },
    [SERVICE_ENUM_MASTER_NLONGRUN] = { .field = "nlongrun" },
    [SERVICE_ENUM_MASTER_NONESHOT] = { .field = "noneshot" },
    [SERVICE_ENUM_MASTER_NMODULE] = { .field = "nmodule" },
    [SERVICE_ENUM_MASTER_ENDOFKEY] = { .field = 0 },
} ;

int service_resolve_master_modify_field(resolve_service_master_t *mres, uint8_t field, char const *data)
{
    log_flow() ;

    uint32_t ifield ;
    int e = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_MASTER, mres) ;

    switch(field) {

        case SERVICE_ENUM_MASTER_NAME:
            mres->name = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_MASTER_CLASSIC:
            mres->classic = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_MASTER_BUNDLE:
            mres->bundle = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_MASTER_LONGRUN:
            mres->longrun = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_MASTER_ONESHOT:
            mres->oneshot = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_MASTER_MODULE:
            mres->module = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_MASTER_NCLASSIC:
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->nclassic= ifield ;
            break ;

        case SERVICE_ENUM_MASTER_NBUNDLE:
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->nbundle = ifield ;
            break ;

        case SERVICE_ENUM_MASTER_NLONGRUN:
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->nlongrun = ifield ;
            break ;

        case SERVICE_ENUM_MASTER_NONESHOT:
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->noneshot = ifield ;
            break ;

        case SERVICE_ENUM_MASTER_NMODULE:
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->nmodule = ifield ;
            break ;

        default:
            break ;
    }

    e = 1 ;

    err:
        free(wres) ;
        return e ;

}
