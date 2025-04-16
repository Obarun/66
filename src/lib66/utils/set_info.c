/*
 * set_info.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
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

#include <skalibs/stralloc.h>

#include <66/ssexec.h>
#include <66/utils.h>


void set_info(ssexec_t *info)
{
    log_flow() ;

    int r ;

    if (!info->skip_opt_tree)
        set_treeinfo(info) ;

    r = set_livedir(&info->live) ;
    if (!r)
        log_die_nomem("stralloc") ;
    if(r < 0)
        log_die(LOG_EXIT_SYS, "live: ", info->live.s, " must be an absolute path") ;

    if (!stralloc_copy(&info->scandir, &info->live))
        log_die_nomem("stralloc") ;

    r = set_livescan(&info->scandir, info->owner) ;
    if (!r)
        log_die_nomem("stralloc") ;
    if(r < 0)
        log_die(LOG_EXIT_SYS, "scandir: ", info->scandir.s, " must be an absolute path") ;

    if (!set_environment(&info->environment, info->owner))
        log_dieusys(LOG_EXIT_ZERO, "set environment") ;
}