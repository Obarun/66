/*
 * tree_ongroups.c
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
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/tree.h>

int tree_ongroups(char const *base, char const *treename, char const *group)
{
    log_flow() ;

    int e = -1 ;
    size_t pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

    if (resolve_read_g(wres, base, treename) <= 0)
        goto err ;

    if (tres.ngroups) {

        if (!sastr_clean_string(&sa, tres.sa.s + tres.groups))
            goto err ;

        e = 0 ;

        FOREACH_SASTR(&sa, pos) {

            if (!strcmp(group, sa.s + pos)) {
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
