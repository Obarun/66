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

    [E_RESOLVE_SERVICE_MASTER_NAME] = { .field = "name" },
    [E_RESOLVE_SERVICE_MASTER_CLASSIC] = { .field = "classic" },
    [E_RESOLVE_SERVICE_MASTER_BUNDLE] = { .field = "bundle" },
    [E_RESOLVE_SERVICE_MASTER_ONESHOT] = { .field = "oneshot" },
    [E_RESOLVE_SERVICE_MASTER_MODULE] = { .field = "module" },
    [E_RESOLVE_SERVICE_MASTER_ENABLED] = { .field = "enabled" },
    [E_RESOLVE_SERVICE_MASTER_DISABLED] = { .field = "disabled" },
    [E_RESOLVE_SERVICE_MASTER_CONTENTS] = { .field = "contents" },

    [E_RESOLVE_SERVICE_MASTER_NCLASSIC] = { .field = "nclassic" },
    [E_RESOLVE_SERVICE_MASTER_NBUNDLE] = { .field = "nbundle" },
    [E_RESOLVE_SERVICE_MASTER_NONESHOT] = { .field = "noneshot" },
    [E_RESOLVE_SERVICE_MASTER_NMODULE] = { .field = "nmodule" },
    [E_RESOLVE_SERVICE_MASTER_NENABLED] = { .field = "nenabled" },
    [E_RESOLVE_SERVICE_MASTER_NDISABLED] = { .field = "ndisabled" },
    [E_RESOLVE_SERVICE_MASTER_NCONTENTS] = { .field = "ncontents" },
    [E_RESOLVE_SERVICE_MASTER_ENDOFKEY] = { .field = 0 },
} ;

int service_resolve_master_modify_field(resolve_service_master_t *mres, uint8_t field, char const *data)
{
    log_flow() ;

    uint32_t ifield ;
    int e = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_MASTER, mres) ;

    switch(field) {

        case E_RESOLVE_SERVICE_MASTER_NAME:
            mres->name = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_CLASSIC:
            mres->classic = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_BUNDLE:
            mres->bundle = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_ONESHOT:
            mres->oneshot = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_MODULE:
            mres->module = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_ENABLED:
            mres->enabled = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_DISABLED:
            mres->disabled = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_CONTENTS:
            mres->contents = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NCLASSIC:
            if (!data)
                data = "0" ;
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->nclassic= ifield ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NBUNDLE:
            if (!data)
                data = "0" ;
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->nbundle = ifield ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NONESHOT:
            if (!data)
                data = "0" ;
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->noneshot = ifield ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NMODULE:
            if (!data)
                data = "0" ;
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->nmodule = ifield ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NENABLED:
            if (!data)
                data = "0" ;
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->nenabled = ifield ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NDISABLED:
            if (!data)
                data = "0" ;
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->ndisabled = ifield ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NCONTENTS:
            if (!data)
                data = "0" ;
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->ncontents = ifield ;
            break ;

        default:
            break ;
    }

    e = 1 ;

    err:
        free(wres) ;
        return e ;

}
