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
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/ssexec.h>
#include <66/utils.h>


void set_info(ssexec_t *info)
{
    log_flow() ;

    if (!info->skip_opt_tree)
        set_treeinfo(info) ;

    info->live.len = 0 ;

    if (!auto_strbuf(&info->live, SS_LIVE))
        log_die_nomem("strbuf") ;

    if (!set_livescan(&info->scandir, info->owner))
        log_die_nomem("strbuf") ;

    if (!set_environment(&info->environment, info->owner))
        log_dieusys(LOG_EXIT_ZERO, "set environment") ;
}