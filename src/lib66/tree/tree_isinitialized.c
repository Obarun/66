/*
 * tree_isinitialized.c
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

#include <string.h>

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/tree.h>

int tree_isinitialized(char const *base, char const *treename)
{
    log_flow() ;

    int e = -1 ;
    size_t baselen = strlen(base) ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    char solve[baselen + SS_SYSTEM_LEN + 1] ;

    auto_strings(solve, base, SS_SYSTEM) ;

    if (!resolve_read(wres, solve, treename))
        goto err ;

    if (tres.init)
        e = 1 ;
    else
        e = 0 ;

    err:
        resolve_free(wres) ;
        return e ;
}
