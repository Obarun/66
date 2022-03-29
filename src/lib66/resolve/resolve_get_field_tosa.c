/*
 * resolve_get_field_tosa.c
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
#include <stdint.h>
#include <stdlib.h>

#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/tree.h>
#include <66/service.h>
#include <66/constants.h>

int resolve_get_field_tosa(stralloc *sa, resolve_wrapper_t_ref wres, uint8_t field)
{
    log_flow() ;

    int e = 0 ;
    resolve_wrapper_t_ref mwres = 0 ;

    if (wres->type == DATA_SERVICE) {

        resolve_service_t_ref res = (resolve_service_t *)wres->obj  ;
        mwres = resolve_set_struct(DATA_SERVICE, res) ;

        if (!service_resolve_get_field_tosa(sa, res, field))
            goto err ;

    } else if (wres->type == DATA_SERVICE_MASTER) {

        resolve_service_master_t_ref res = (resolve_service_master_t *)wres->obj  ;
        mwres = resolve_set_struct(DATA_SERVICE_MASTER, res) ;

        if (!service_resolve_master_get_field_tosa(sa, res, field))
            goto err ;

    } else if (wres->type == DATA_TREE) {

        resolve_tree_t_ref res = (resolve_tree_t *)wres->obj  ;
        mwres = resolve_set_struct(DATA_TREE, res) ;

        if (!tree_resolve_get_field_tosa(sa, res, field))
            goto err ;

    } else if (wres->type == DATA_TREE_MASTER) {

        resolve_tree_master_t_ref res = (resolve_tree_master_t *)wres->obj  ;
        mwres = resolve_set_struct(DATA_TREE_MASTER, res) ;

        if (!tree_resolve_master_get_field_tosa(sa, res, field))
            goto err ;
    }

    e = 1 ;
    err:
        free(mwres) ;
        return e ;

}
