/*
 * state_read.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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

#include <66/state.h>
#include <66/service.h>

int state_read(ss_state_t *sta, resolve_service_t const *res)
{
    log_flow() ;

    // the live copy first, then the one the parse writes in <base>
    int r = state_read_remote(sta, res->sa.s + res->live.statedir) ;
    if (!r)
        r = state_read_file(sta, res->sa.s + res->live.status) ;

    // the absence of both is told by 0, errno ENOENT
    if (!r)
        errno = ENOENT ;

    return r ;
}
