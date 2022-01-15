/*
 * resolve_modify_field.c
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

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/graph.h>

int resolve_modify_field(resolve_wrapper_t_ref wres, uint8_t field, char const *by)
{
    log_flow() ;

    int e = 0 ;

    resolve_wrapper_t_ref mwres = 0 ;

    if (wres->type == DATA_SERVICE) {

        resolve_service_t_ref res = (resolve_service_t *)wres->obj  ;
        mwres = resolve_set_struct(DATA_SERVICE, res) ;

        log_trace("modify field ", resolve_service_field_table[field].field," of service ", res->sa.s + res->name, " with value: ", by) ;

        if (!service_resolve_modify_field(res, field, by))
            goto err ;

    } else if (wres->type == DATA_TREE) {

        resolve_tree_t_ref res = (resolve_tree_t *)wres->obj  ;
        mwres = resolve_set_struct(DATA_TREE, res) ;

        log_trace("modify field ", resolve_tree_field_table[field].field," of tree ", res->sa.s + res->name, " with value: ", by) ;

        if (!tree_resolve_modify_field(res, field, by))
            goto err ;

    } else if (wres->type == DATA_TREE_MASTER) {

        resolve_tree_master_t_ref res = (resolve_tree_master_t *)wres->obj  ;
        mwres = resolve_set_struct(DATA_TREE_MASTER, res) ;

        log_trace("modify field ", resolve_tree_master_field_table[field].field," of inner resolve file of trees with value: ", by) ;

        if (!tree_resolve_master_modify_field(res, field, by))
            goto err ;
    }

    e = 1 ;
    err:
        free(mwres) ;
        return e ;
}
