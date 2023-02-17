/*
 * tree_write_cdb.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stddef.h>

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/tree.h>

int tree_resolve_copy(resolve_tree_t *dst, resolve_tree_t *tres)
{
    log_flow() ;

    stralloc_free(&dst->sa) ;

    size_t len = tres->sa.len - 1 ;
    dst->salen = tres->salen ;

    if (!stralloc_catb(&dst->sa,tres->sa.s,len) ||
        !stralloc_0(&dst->sa))
            return 0 ;

    dst->name = tres->name ;
    dst->depends = tres->depends ;
    dst->requiredby = tres->requiredby ;
    dst->allow = tres->allow ;
    dst->groups = tres->groups ;
    dst->contents = tres->contents ;
    dst->ndepends = tres->ndepends ;
    dst->nrequiredby = tres->nrequiredby ;
    dst->nallow = tres->nallow ;
    dst->ngroups = tres->ngroups ;
    dst->ncontents = tres->ncontents ;
    dst->init = tres->init ;
    dst->supervised = tres->supervised ;

    return 1 ;

}
