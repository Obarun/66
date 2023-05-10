/*
 * set_treeinfo.c
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

#include <stddef.h>

#include <oblibs/log.h>

#include <66/utils.h>
#include <66/ssexec.h>
#include <66/tree.h>

void set_treeinfo(ssexec_t *info)
{
    log_flow() ;

    int r = tree_sethome(info) ;
    if (r == -1)
        log_dieu(LOG_EXIT_USER, "set the tree name") ;
    if (!r)
        log_dieu(LOG_EXIT_USER, "parse seed file") ;

    if (!tree_get_permissions(info->base.s, info->treename.s))
        log_die(LOG_EXIT_USER,"You're not allowed to use the tree: ",info->treename.s) ;

    info->treeallow = 1 ;
}
