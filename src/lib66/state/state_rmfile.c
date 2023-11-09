/*
 * state_rmfile.c
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

#include <string.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/posixplz.h>

#include <66/state.h>
#include <66/constants.h>
#include <66/service.h>

void state_rmfile(resolve_service_t *res)
{
    log_flow() ;

    char status[strlen(res->sa.s + res->live.statedir) + 1 + SS_STATUS_LEN + 1] ;

    auto_strings(status, res->sa.s + res->live.statedir, "/", SS_STATUS) ;

    if (access(status, F_OK) < 0) {
        unlink_void(res->sa.s + res->live.status) ;
    } else {
        unlink_void(status) ;
    }
}
