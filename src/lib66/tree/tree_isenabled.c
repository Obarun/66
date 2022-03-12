/*
 * tree_isenabled.c
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

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/tree.h>

int tree_isenabled(char const *base, char const *treename)
{
    log_flow() ;

    int e = -1 ;
    size_t baselen = strlen(base), pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;
    char solve[baselen + SS_SYSTEM_LEN + 1] ;

    auto_strings(solve, base, SS_SYSTEM) ;

    if (!resolve_read(wres, solve, SS_MASTER + 1))
        goto err ;

    if (mres.nenabled) {

        if (!sastr_clean_string(&sa, mres.sa.s + mres.enabled))
            goto err ;

        e = 0 ;

        FOREACH_SASTR(&sa, pos) {

            if (!strcmp(treename, sa.s + pos)) {
                e = 1 ;
                break ;
            }
        }
    }
    else e = 0 ;

    err:
        resolve_free(wres) ;
        stralloc_free(&sa) ;
        return e ;
}
