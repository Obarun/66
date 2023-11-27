/*
 * tree_iscurrent.c
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

#include <string.h>

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/tree.h>

int tree_iscurrent(char const *base, char const *treename)
{
    log_flow() ;

    int e = -1 ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

    if (resolve_read_g(wres, base, SS_MASTER + 1)  <= 0)
        goto err ;

    if (!strcmp(mres.sa.s + mres.current, treename))
        e = 1 ;
    else
        e = 0 ;

    err:
        resolve_free(wres) ;
        return e ;
}
