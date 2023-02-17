/*
 * tree_isvalid.c
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

#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/tree.h>
#include <66/resolve.h>
#include <66/constants.h>

int tree_isvalid(char const *base, char const *treename)
{
    log_flow() ;

    int e = -1 ;
    size_t pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

    /** make distinction between system error
     * and unexisting tree */
    {
        resolve_tree_t tres = RESOLVE_TREE_ZERO ;
        resolve_wrapper_t_ref twres = resolve_set_struct(DATA_TREE, &tres) ;

        if (!resolve_check_g(twres, base, treename)) {
            e = 0 ;
            goto freed ;
        }
        resolve_free(twres) ;
    }

    if (!resolve_read_g(wres, base, SS_MASTER + 1))
        goto freed ;

    if (!tree_resolve_master_get_field_tosa(&sa, &mres, E_RESOLVE_TREE_MASTER_CONTENTS))
        goto freed ;

    if (mres.ncontents) {

        if (!sastr_clean_string_flush_sa(&sa, sa.s))
            goto freed ;

        FOREACH_SASTR(&sa, pos) {

            if (!strcmp(treename, sa.s + pos)) {
                e = 1 ;
                goto freed ;
            }
        }

    } else e = 0 ;

    freed:
        stralloc_free(&sa) ;
        resolve_free(wres) ;
        return e ;

}
