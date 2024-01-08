/*
 * resolve_get_field_tosa.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>

int resolve_get_field_tosa(stralloc *sa, resolve_wrapper_t_ref wres, uint8_t field)
{
    log_flow() ;

    if (wres->type == DATA_SERVICE) {

        resolve_service_t_ref res = (resolve_service_t *)wres->obj  ;

        return service_resolve_get_field_tosa(sa, res, field) ;

    } else if (wres->type == DATA_TREE) {

        resolve_tree_t_ref res = (resolve_tree_t *)wres->obj  ;

        return tree_resolve_get_field_tosa(sa, res, field) ;

    } else if (wres->type == DATA_TREE_MASTER) {

        resolve_tree_master_t_ref res = (resolve_tree_master_t *)wres->obj  ;

        return tree_resolve_master_get_field_tosa(sa, res, field) ;

    } else return 0 ;
}
