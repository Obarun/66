/*
 * tree_resolve_master_sanitize.c
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

#include <string.h>
#include <stdlib.h>

#include <oblibs/log.h>

#include <66/tree.h>
#include <66/resolve.h>

void tree_resolve_master_sanitize(resolve_tree_master_t *mres)
{
    log_flow() ;

    char stk[mres->sa.len + 1] ;

    memcpy(stk, mres->sa.s, mres->sa.len) ;
    stk[mres->sa.len] = 0 ;

    mres->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, mres) ;

    resolve_init(wres) ;

    mres->name = mres->name ? resolve_add_string(wres, stk + mres->name) : 0 ;
    mres->allow = mres->allow ? resolve_add_string(wres, stk + mres->allow) : 0 ;
    mres->current = mres->current ? resolve_add_string(wres, stk + mres->current) : 0 ;
    mres->contents = mres->contents ? resolve_add_string(wres, stk + mres->contents) : 0 ;
    mres->rversion = mres->rversion ? resolve_add_string(wres, stk + mres->rversion) : 0 ;
    free(wres) ;
}
