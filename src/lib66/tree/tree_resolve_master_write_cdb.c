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

static void add_version(resolve_tree_master_t *mres)
{
    log_flow() ;
    log_trace("Master resolve file version set to: ", SS_VERSION) ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, mres) ;
    mres->rversion = resolve_add_string(wres, SS_VERSION) ;
    free(wres) ;
}

int tree_resolve_master_write_cdb(cdbmaker *c, resolve_tree_master_t *mres)
{
    log_flow() ;

    add_version(mres) ;

    if (!cdbmake_add(c, "sa", 2, mres->sa.s, mres->sa.len))
        return 0 ;

    if (!resolve_add_cdb_uint(c, "rversion", mres->rversion) ||
        !resolve_add_cdb_uint(c, "name", mres->name) ||
        !resolve_add_cdb_uint(c, "allow", mres->allow) ||
        !resolve_add_cdb_uint(c, "current", mres->current) ||
        !resolve_add_cdb_uint(c, "contents", mres->contents) ||
        !resolve_add_cdb_uint(c, "nallow", mres->nallow) ||
        !resolve_add_cdb_uint(c, "ncontents", mres->ncontents))
            return 0 ;

    return 1 ;
}
