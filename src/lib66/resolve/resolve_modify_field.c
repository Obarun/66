/*
 * resolve_modify_field.c
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

#include <stdint.h>

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>

int resolve_modify_field(resolve_wrapper_t_ref wres, uint8_t field, char const *by)
{
    log_flow() ;

    if (wres->type == DATA_SERVICE) {

        resolve_service_t_ref res = (resolve_service_t *)wres->obj  ;

        log_trace("modify field ", resolve_service_field_table[field].field," of service ", res->sa.s + res->name, " with value: ", by) ;

        return service_resolve_modify_field(res, field, by) ;

    } else if (wres->type == DATA_TREE) {

        resolve_tree_t_ref res = (resolve_tree_t *)wres->obj  ;

        log_trace("modify field ", resolve_tree_field_table[field].field," of tree ", res->sa.s + res->name, " with value: ", by) ;

        return tree_resolve_modify_field(res, field, by) ;

    } else if (wres->type == DATA_TREE_MASTER) {

        resolve_tree_master_t_ref res = (resolve_tree_master_t *)wres->obj  ;

        log_trace("modify field ", resolve_tree_master_field_table[field].field," of resolve Master file of trees with value: ", by) ;

        return tree_resolve_master_modify_field(res, field, by) ;

    } else return 0 ;
}
