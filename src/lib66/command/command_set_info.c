/*
 * command_set_info.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/command.h>

void command_set_info(ssexec_t *info)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;

    int r = set_info(info, &msg) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "set the information of the command") ;

    if (!r)
        log_die(LOG_EXIT_USER, msg.s) ;
}
