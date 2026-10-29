/*
 * tree_resolve_master_read_cdb.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/cdb.h>

#include <66/resolve.h>
#include <66/tree.h>

int tree_resolve_master_read_cdb(ocdb *c, resolve_tree_master_t *mres)
{
    log_flow() ;

    if (resolve_get_sa(&mres->sa,c) <= 0) {
        return (errno = EINVAL, 0)  ;
    }

    /* configuration */
    if (!resolve_get_key_u32(c, "rversion", &mres->rversion)) {
        return (errno = EINVAL, 0)  ;
    }

    if (!resolve_get_key_u32(c, "name", &mres->name) ||
        !resolve_get_key_u32(c, "allow", &mres->allow) ||
        !resolve_get_key_u32(c, "current", &mres->current) ||
        !resolve_get_key_u32(c, "contents", &mres->contents) ||
        !resolve_get_key_u32(c, "nallow", &mres->nallow) ||
        !resolve_get_key_u32(c, "ncontents", &mres->ncontents)) {
            return 0 ;
    }

    return 1 ;
}
