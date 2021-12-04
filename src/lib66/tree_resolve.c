/*
 * tree_resolve.c
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
#include <stddef.h>
#include <stdlib.h>//free

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdbmake.h>
#include <skalibs/cdb.h>

#include <66/tree.h>
#include <66/resolve.h>

int tree_read_cdb(cdb *c, resolve_tree_t *tres)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres ;
    uint32_t x ;

    wres = resolve_set_struct(SERVICE_STRUCT, tres) ;

    resolve_init(wres) ;

    /* name */
    resolve_find_cdb(&tmp,c,"name") ;
    tres->name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* depends */
    resolve_find_cdb(&tmp,c,"depends") ;
    tres->depends = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* requiredby */
    resolve_find_cdb(&tmp,c,"requiredby") ;
    tres->requiredby = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* allow */
    resolve_find_cdb(&tmp,c,"allow") ;
    tres->allow = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* contents */
    resolve_find_cdb(&tmp,c,"contents") ;
    tres->contents = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* ndepends */
    x = resolve_find_cdb(&tmp,c,"ndepends") ;
    tres->ndepends = x ;

    /* nrequiredby */
    x = resolve_find_cdb(&tmp,c,"nrequiredby") ;
    tres->nrequiredby = x ;

    /* ncontents */
    x = resolve_find_cdb(&tmp,c,"ncontents") ;
    tres->ncontents = x ;

    /* current */
    x = resolve_find_cdb(&tmp,c,"current") ;
    tres->current = x ;

    /* init */
    x = resolve_find_cdb(&tmp,c,"init") ;
    tres->init = x ;

    /* disen */
    x = resolve_find_cdb(&tmp,c,"disen") ;
    tres->disen = x ;

    free(wres) ;
    stralloc_free(&tmp) ;

    return 1 ;
}

int tree_write_cdb(cdbmaker *c, resolve_tree_t *tres)
{
    log_flow() ;

    char *str = tres->sa.s ;

    /* name */
    if (!resolve_add_cdb(c,"name",str + tres->name) ||

    /* depends */
    !resolve_add_cdb(c,"depends",str + tres->depends) ||

    /* requiredby */
    !resolve_add_cdb(c,"requiredby",str + tres->requiredby) ||

    /* allow */
    !resolve_add_cdb(c,"allow",str + tres->allow) ||

    /* contents */
    !resolve_add_cdb(c,"contents",str + tres->contents) ||

    /* ndepends */
    !resolve_add_cdb_uint(c,"ndepends",tres->ndepends) ||

    /* nrequiredby */
    !resolve_add_cdb_uint(c,"nrequiredby",tres->nrequiredby) ||

    /* ncontents */
    !resolve_add_cdb_uint(c,"ncontents",tres->ncontents) ||

    /* current */
    !resolve_add_cdb_uint(c,"current",tres->current) ||

    /* init */
    !resolve_add_cdb_uint(c,"init",tres->init) ||

    /* disen */
    !resolve_add_cdb_uint(c,"disen",tres->disen)) return 0 ;

    return 1 ;
}

int tree_resolve_copy(resolve_tree_t *dst, resolve_tree_t *tres)
{
    log_flow() ;

    stralloc_free(&dst->sa) ;

    size_t len = tres->sa.len - 1 ;
    dst->salen = tres->salen ;

    if (!stralloc_catb(&dst->sa,tres->sa.s,len) ||
        !stralloc_0(&dst->sa))
            return 0 ;

    dst->name = tres->name ;
    dst->depends = tres->depends ;
    dst->requiredby = tres->requiredby ;
    dst->allow = tres->allow ;
    dst->contents = tres->contents ;
    dst->ndepends = tres->ndepends ;
    dst->nrequiredby = tres->nrequiredby ;
    dst->ncontents = tres->ncontents ;
    dst->current = tres->current ;
    dst->init = tres->init ;
    dst->disen = tres->disen ;

    return 1 ;

}
