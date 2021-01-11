/*
 * tree_sethome.c
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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

#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>

#include <oblibs/types.h>
#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>

int tree_sethome(stralloc *tree, char const *base,uid_t owner)
{
    log_flow() ;

    int r ;

    if (!tree->len)
    {
        if (!tree_find_current(tree, base,owner)) return -1 ;
    }
    else
    {
        char treename[tree->len + 1] ;
        memcpy(treename,tree->s,tree->len) ;
        treename[tree->len] = 0 ;
        tree->len = 0 ;
        if (!stralloc_cats(tree,base) ||
        !stralloc_cats(tree,SS_SYSTEM "/") ||
        !stralloc_cats(tree,treename) ||
        !stralloc_0(tree)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
        r = scan_mode(tree->s,S_IFDIR) ;
        if (r < 0) errno = EEXIST ;
        if (r != 1) return 0 ;
        tree->len--;
    }

    return 1 ;
}
