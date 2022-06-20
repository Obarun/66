/*
 * create_live_tree.c
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

#include <sys/types.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/constants.h>

int create_live_tree(ssexec_t *info)
{
    log_flow() ;

    int r = scan_mode(info->livetree.s, S_IFDIR) ;
    if (r < 0)
        log_warn_return(LOG_EXIT_ZERO, "conflicting format for: ", info->livetree.s) ;
    if (!r) {

        log_trace("create directory: ", info->livetree.s) ;
        if (!dir_create(info->livetree.s, 0700))
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", info->livetree.s) ;

        log_trace("chown directory: ", info->livetree.s) ;
        if (chown(info->livetree.s,info->owner,gidowner) < 0)
            log_warnusys_return(LOG_EXIT_SYS, "chown directory: ", info->livetree.s) ;
    }

    return 1 ;
}
