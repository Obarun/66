/*
 * tree_resolve_master_read_cdb.c
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
#include <stdlib.h>//free

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>

#include <66/tree.h>
#include <66/resolve.h>

int tree_resolve_master_read_cdb(cdb *c, resolve_tree_master_t *mres)
{
    log_flow() ;

    resolve_wrapper_t_ref wres ;

    wres = resolve_set_struct(DATA_TREE_MASTER, mres) ;

    if (resolve_get_sa(&mres->sa,c) <= 0 || !mres->sa.len)
        return 0 ;

    if (!resolve_get_key(c, "name", &mres->name) ||
        !resolve_get_key(c, "allow", &mres->allow) ||
        !resolve_get_key(c, "current", &mres->current) ||
        !resolve_get_key(c, "contents", &mres->contents) ||
        !resolve_get_key(c, "nallow", &mres->nallow) ||
        !resolve_get_key(c, "ncontents", &mres->ncontents)) {
            free(wres) ;
            return 0 ;
    }

    free(wres) ;

    return 1 ;
}
