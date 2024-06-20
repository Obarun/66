/*
 * tree_resolve_master_modify_field.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

    [E_RESOLVE_TREE_MASTER_NAME] = { .field = "name" },
    [E_RESOLVE_TREE_MASTER_ALLOW] = { .field = "allow" },
    [E_RESOLVE_TREE_MASTER_CURRENT] = { .field = "current" },
    [E_RESOLVE_TREE_MASTER_CONTENTS] = { .field = "contents" },
    [E_RESOLVE_TREE_MASTER_NALLOW] = { .field = "nallow" },
    [E_RESOLVE_TREE_MASTER_NCONTENTS] = { .field = "ncontents" },
    [E_RESOLVE_TREE_MASTER_ENDOFKEY] = { .field = 0 }
} ;

static uint32_t resolve_add_uint(char const *data)
{
    uint32_t u ;

    if (!data)
        data = "0" ;
    if (!uint0_scan(data, &u))
        return 0 ;
    return u ;
}

void tree_resolve_master_modify_field(resolve_tree_master_t *mres, uint8_t field, char const *data)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, mres) ;

    switch(field) {

        case E_RESOLVE_TREE_MASTER_NAME:
            mres->name = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_TREE_MASTER_ALLOW:
            mres->allow = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_TREE_MASTER_CURRENT:
            mres->current = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_TREE_MASTER_CONTENTS:
            mres->contents = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_TREE_MASTER_NALLOW:
            mres->nallow = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_TREE_MASTER_NCONTENTS:
            mres->ncontents = resolve_add_uint(data) ;
            break ;

        default:
            break ;
    }

    tree_resolve_master_sanitize(mres) ;
    free(wres) ;
}
