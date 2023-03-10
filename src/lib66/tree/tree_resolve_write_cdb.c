/*
 * tree_resolve_write_cdb.c
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

#include <oblibs/log.h>

#include <skalibs/cdbmake.h>

#include <66/tree.h>
#include <66/resolve.h>

int tree_resolve_write_cdb(cdbmaker *c, resolve_tree_t *tres)
{
    log_flow() ;

    char *str = tres->sa.s ;

    /* name */
    if (!resolve_add_cdb(c,"name",str, tres->name, 0) ||

    /* depends */
    !resolve_add_cdb(c,"depends",str, tres->depends, 1) ||

    /* requiredby */
    !resolve_add_cdb(c,"requiredby",str, tres->requiredby, 1) ||

    /* allow */
    !resolve_add_cdb(c,"allow",str, tres->allow, 1) ||

    /* groups */
    !resolve_add_cdb(c,"groups",str, tres->groups, 1) ||

    /* contents */
    !resolve_add_cdb(c,"contents",str, tres->contents, 1) ||

    /* ndepends */
    !resolve_add_cdb_uint(c,"ndepends",tres->ndepends) ||

    /* nrequiredby */
    !resolve_add_cdb_uint(c,"nrequiredby",tres->nrequiredby) ||

    /* nallow */
    !resolve_add_cdb_uint(c,"nallow",tres->nallow) ||

    /* ngroups */
    !resolve_add_cdb_uint(c,"ngroups",tres->ngroups) ||

    /* ncontents */
    !resolve_add_cdb_uint(c,"ncontents",tres->ncontents) ||

    /* init */
    !resolve_add_cdb_uint(c,"init",tres->init) ||

    /* supervised */
    !resolve_add_cdb_uint(c,"supervised",tres->supervised)) return 0 ;

    return 1 ;
}
