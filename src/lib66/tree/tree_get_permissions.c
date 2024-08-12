/*
 * tree_get_permissions.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <unistd.h>
#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/utils.h>
#include <66/tree.h>
#include <66/resolve.h>
#include <66/constants.h>

int tree_get_permissions(char const *base, char const *treename)
{
    log_flow() ;

    int e = -1 ;
    size_t pos = 0 ;
    _alloc_sa_(sa) ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    uid_t uid = getuid(), treeuid = -1 ;

    if (resolve_read_g(wres, base, treename) <= 0)
        goto freed ;

    if (tres.nallow) {

        if (!sastr_clean_string(&sa, tres.sa.s + tres.allow))
            goto freed ;

        FOREACH_SASTR(&sa, pos) {

            if (!uint0_scan(sa.s + pos, &treeuid))
                goto freed ;

            if (uid == treeuid) {
                e = 1 ;
                goto freed ;
            }
        }

    } else {

        e = 0 ;
        goto freed ;
    }

    freed:
        resolve_free(wres) ;
        return e ;
}
