/*
 * tree_seed_setseed.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/tree.h>

int tree_seed_setseed(tree_seed_t *seed, char const *treename)
{
    log_flow() ;

    int e = 0 ;
    _cleanup_strbuf_ strbuf src = STRBUF_ZERO ;

    if (!tree_seed_resolve_path(&src, treename))
        goto err ;

    seed->name = seed->sa.len ;
    if (!sbl_add(&seed->sa, treename))
        goto err ;

    if (!tree_seed_parse_file(seed, src.s) ||
        !tree_seed_get_group_permissions(seed))
            goto err ;

    e = 1 ;
    err:
        return e ;
}
