/*
 * command_refuse_scandir.c
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

#include <66/ssexec.h>
#include <66/svc.h>
#include <66/command.h>

void command_refuse_scandir(ssexec_t const *info)
{
    log_flow() ;

    int r = svc_scandir_ok(info->scandir.s) ;

    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check the scandir: ", info->scandir.s) ;

    if (!r)
        log_die(LOG_EXIT_USER, "scandir: ", info->scandir.s, " is not running") ;
}
