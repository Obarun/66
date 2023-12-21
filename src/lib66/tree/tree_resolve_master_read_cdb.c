/*
 * tree_resolve_master_read_cdb.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>//free

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>

#include <66/tree.h>
#include <66/resolve.h>

int tree_resolve_master_read_cdb(cdb *c, resolve_tree_master_t *mres)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres ;
    uint32_t x ;

    wres = resolve_set_struct(DATA_TREE_MASTER, mres) ;

    resolve_init(wres) ;

    /* name */
    resolve_find_cdb(&tmp,c,"name") ;
    mres->name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* allow */
    resolve_find_cdb(&tmp,c,"allow") ;
    mres->allow = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* current */
    resolve_find_cdb(&tmp,c,"current") ;
    mres->current = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* contents */
    resolve_find_cdb(&tmp,c,"contents") ;
    mres->contents = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* nallow */
    x = resolve_find_cdb(&tmp,c,"nallow") ;
    mres->nallow = x ;

    /* ncontents */
    x = resolve_find_cdb(&tmp,c,"ncontents") ;
    mres->ncontents = x ;

    free(wres) ;
    stralloc_free(&tmp) ;

    return 1 ;
}
