/*
 * tree_resolve_master_modify_field.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <oblibs/types.h>

#include <66/tree.h>
#include <66/resolve.h>

static uint32_t resolve_add_uint(char const *data)
{
    uint32_t u ;

    if (!data)
        data = "0" ;
    if (!u32_scan_strict(data, &u))
        return 0 ;
    return u ;
}

void tree_resolve_master_modify_field(resolve_tree_master_t *mres, uint32_t field, char const *data)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, mres) ;

    switch(field) {

        case E_RESOLVE_TREE_MASTER_RVERSION:
            mres->rversion = resolve_add_string(wres, data) ;
            break ;

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
