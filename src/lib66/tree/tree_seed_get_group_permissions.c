/*
 * tree_seed_ismandatory.c
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

#include <unistd.h>//getuid
#include <string.h>

#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/tree.h>

int tree_seed_get_group_permissions(tree_seed_t *seed)
{
    log_flow() ;

    int e = 0 ;
    uid_t uid = getuid() ;

    stralloc sv = STRALLOC_ZERO ;

    char *groups = seed->sa.s + seed->groups ;

    if (!uid && (!strcmp(groups, TREE_GROUPS_USER))) {

        log_warn("Only regular user can use this seed") ;
        goto err ;

    } else if (uid && (strcmp(groups, TREE_GROUPS_USER))) {

        log_warn("Only root user can use seed: ", seed->sa.s + seed->name) ;
        goto err ;
    }

    if (!strcmp(groups, TREE_GROUPS_BOOT) && seed->disen > 0) {

        log_1_warn("enable was asked for a tree on group boot -- ignoring enable request") ;
        seed->disen = 0 ;
    }

    e = 1 ;
    err:
        stralloc_free(&sv) ;
        return e ;
}
