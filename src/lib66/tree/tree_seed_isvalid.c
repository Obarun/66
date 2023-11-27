/*
 * tree_seed_isvalid.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

#include <skalibs/stralloc.h>

#include <66/tree.h>

int tree_seed_isvalid(char const *seed)
{
    log_flow() ;

    int e = 1 ;
    stralloc src = STRALLOC_ZERO ;

    if (!tree_seed_resolve_path(&src, seed))
        e = 0 ;

    stralloc_free(&src) ;

    return e ;
}
