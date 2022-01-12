/*
 * tree_switch_current.c
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

#include <66/tree.h>

#include <sys/types.h>
#include <string.h>

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <66/constants.h>
#include <66/resolve.h>

int tree_switch_current(char const *base, char const *treename)
{
    log_flow() ;

    ssize_t r ;
    size_t baselen = strlen(base), treelen = strlen(treename) ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

    if (!resolve_modify_field_g(wres, base, SS_MASTER + 1, TREE_ENUM_MASTER_CURRENT,  treename))
        log_warnu_return(LOG_EXIT_ZERO, "modify field: ", resolve_tree_master_field_table[TREE_ENUM_MASTER_CURRENT].field," of inner resolve file with value: ", treename) ;

    resolve_free(wres) ;

    return 1 ;
}
