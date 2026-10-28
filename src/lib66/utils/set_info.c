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
 * except according to the terms contained in the LICENSE file.
 */

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/ssexec.h>
#include <66/utils.h>


int set_info(ssexec_t *info, strbuf *msg)
{
    log_flow() ;

    if (!info->skip_opt_tree) {

        int r = set_treeinfo(info, msg) ;
        if (r <= 0)
            return r ;
    }

    info->live.len = 0 ;

    if (!auto_strbuf(&info->live, SS_LIVE) ||
        !set_livescan(&info->scandir, info->owner) ||
        !set_environment(&info->environment, info->owner))
            return -1 ;

    return 1 ;
}
