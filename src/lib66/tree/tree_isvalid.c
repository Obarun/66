/*
 * tree_isvalid.c
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
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref twres = resolve_set_struct(DATA_TREE, &tres) ;

    e = resolve_check_g(twres, base, treename) ;

    resolve_free(twres) ;

    return e ;
}
