/*
 * tree_resolve_read_cdb.c
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

#include <66/resolve.h>
#include <66/tree.h>

int tree_resolve_read_cdb(cdb *c, resolve_tree_t *tres)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres ;
    uint32_t x ;

    wres = resolve_set_struct(DATA_TREE, tres) ;

    resolve_init(wres) ;

    /* name */
    resolve_find_cdb(&tmp,c,"name") ;
    tres->name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* enabled */
    x = resolve_find_cdb(&tmp,c,"enabled") ;
    tres->enabled = x ;

    /* depends */
    resolve_find_cdb(&tmp,c,"depends") ;
    tres->depends = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* requiredby */
    resolve_find_cdb(&tmp,c,"requiredby") ;
    tres->requiredby = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* allow */
    resolve_find_cdb(&tmp,c,"allow") ;
    tres->allow = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* groups */
    resolve_find_cdb(&tmp,c,"groups") ;
    tres->groups = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* contents */
    resolve_find_cdb(&tmp,c,"contents") ;
    tres->contents = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* ndepends */
    x = resolve_find_cdb(&tmp,c,"ndepends") ;
    tres->ndepends = x ;

    /* nrequiredby */
    x = resolve_find_cdb(&tmp,c,"nrequiredby") ;
    tres->nrequiredby = x ;

    /* nallow */
    x = resolve_find_cdb(&tmp,c,"nallow") ;
    tres->nallow = x ;

    /* ngroups */
    x = resolve_find_cdb(&tmp,c,"ngroups") ;
    tres->ngroups = x ;

    /* ncontents */
    x = resolve_find_cdb(&tmp,c,"ncontents") ;
    tres->ncontents = x ;

    /* init */
    x = resolve_find_cdb(&tmp,c,"init") ;
    tres->init = x ;

    /* supervised */
    x = resolve_find_cdb(&tmp,c,"supervised") ;
    tres->supervised = x ;

    free(wres) ;
    stralloc_free(&tmp) ;

    return 1 ;
}
