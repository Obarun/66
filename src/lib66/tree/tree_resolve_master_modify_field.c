/*
 * tree_resolve_master_modify_field.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <stdlib.h>

#include <oblibs/log.h>

#include <skalibs/types.h>

#include <66/tree.h>
#include <66/resolve.h>

resolve_field_table_t resolve_tree_master_field_table[] = {

    [TREE_ENUM_MASTER_NAME] = { .field = "name" },
    [TREE_ENUM_MASTER_ALLOW] = { .field = "allow" },
    [TREE_ENUM_MASTER_ENABLED] = { .field = "enabled" },
    [TREE_ENUM_MASTER_CURRENT] = { .field = "current" },
    [TREE_ENUM_MASTER_NENABLED] = { .field = "nenabled" },
    [TREE_ENUM_MASTER_ENDOFKEY] = { .field = 0 }
} ;

int tree_resolve_master_modify_field(resolve_tree_master_t *mres, uint8_t field, char const *data)
{
    log_flow() ;

    uint32_t ifield ;
    int e = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, mres) ;

    switch(field) {

        case TREE_ENUM_MASTER_NAME:
            mres->name = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_MASTER_ALLOW:
            mres->allow = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_MASTER_ENABLED:
            mres->enabled = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_MASTER_CURRENT:
            mres->current = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_MASTER_NENABLED:
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->nenabled = ifield ;
            break ;

        default:
            break ;
    }

    e = 1 ;

    err:
        free(wres) ;
        return e ;

}
