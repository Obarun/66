/*
 * tree_resolve_sanitize.c
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

void tree_resolve_sanitize(resolve_tree_t *tres)
{
    log_flow() ;

    char stk[tres->sa.len + 1] ;

    memcpy(stk, tres->sa.s, tres->sa.len) ;
    stk[tres->sa.len] = 0 ;

    tres->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, tres) ;

    resolve_init(wres) ;

    tres->name = tres->name ? resolve_add_string(wres, stk + tres->name) : 0 ;
    tres->depends = tres->depends ? resolve_add_string(wres, stk + tres->depends) : 0 ;
    tres->requiredby = tres->requiredby ? resolve_add_string(wres, stk + tres->requiredby) : 0 ;
    tres->allow = tres->allow ? resolve_add_string(wres, stk + tres->allow) : 0 ;
    tres->groups = tres->groups ? resolve_add_string(wres, stk + tres->groups) : 0 ;
    tres->contents = tres->contents ? resolve_add_string(wres, stk + tres->contents) : 0 ;

    free(wres) ;
}
