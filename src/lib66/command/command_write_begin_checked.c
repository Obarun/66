/*
 * command_write_begin_checked.c
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

#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/hash.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/parse.h>
#include <66/graph.h>
#include <66/command.h>

int command_write_begin_checked(hash_t *hres, ssexec_t *info, strbuf *msg)
{
    log_flow() ;

    int r, e ;

    if (service_graph_write_begin(info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "lock the system") ;

    r = parse_check(hres, info, msg) ;
    if (r > 0)
        return 1 ;

    e = errno ;
    command_write_end(0) ;
    errno = e ;

    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check the services to parse") ;

    return 0 ;
}
