/*
 * tree_switch_current.c
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

#include <oblibs/log.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/tree.h>

int tree_switch_current(char const *base, char const *treename)
{
    log_flow() ;

    int e = 0 ;

    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

    if (tree_ongroups(base, treename, TREE_GROUPS_BOOT)) {
        log_1_warn("you can't mark a tree current if it is part of the boot group") ;
        goto freed ;
    }

    if (!resolve_modify_field_g(wres, base, SS_MASTER + 1, E_RESOLVE_TREE_MASTER_CURRENT,  treename)) {
        log_warnu("modify field: ", resolve_tree_master_field_table[E_RESOLVE_TREE_MASTER_CURRENT].field," of Master resolve file with value: ", treename) ;
        goto freed ;
    }

    e = 1 ;

    freed:
        resolve_free(wres) ;
        return e ;
}
