/*
 * tree_resolve_master_write_cdb.c
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

#include <oblibs/log.h>

#include <skalibs/cdbmake.h>

#include <66/tree.h>
#include <66/resolve.h>

int tree_resolve_master_write_cdb(cdbmaker *c, resolve_tree_master_t *mres)
{
    log_flow() ;

    char *str = mres->sa.s ;

    /* name */
    if (!resolve_add_cdb(c,"name",str, mres->name, 1) ||

    /* allow */
    !resolve_add_cdb(c,"allow",str, mres->allow, 1) ||

    /* current */
    !resolve_add_cdb(c,"current",str, mres->current, 1) ||

    /* contents */
    !resolve_add_cdb(c,"contents",str, mres->contents, 1) ||

    /* nallow */
    !resolve_add_cdb_uint(c,"nallow", mres->nallow) ||

    /* ncontents */
    !resolve_add_cdb_uint(c,"ncontents",mres->ncontents)) return 0 ;

    return 1 ;
}
