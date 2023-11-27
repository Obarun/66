/*
 * state_write.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <oblibs/directory.h>

#include <skalibs/djbunix.h>

#include <66/state.h>
#include <66/constants.h>
#include <66/service.h>

int state_write(ss_state_t *sta, resolve_service_t *res)
{
    log_flow() ;

    char pack[STATE_STATE_SIZE] ;
    char *path = 0 ;
    char status[strlen(res->sa.s + res->live.statedir) + 1 + SS_STATUS_LEN + 1] ;

    auto_strings(status, res->sa.s + res->live.statedir, "/", SS_STATUS) ;

    if (access(status, F_OK) < 0) {
        path = res->sa.s + res->live.status ;
    } else {
        path = status ;
    }

    if (access(path, F_OK) < 0) {
        log_trace("create directory: ", path) ;
        if (!dir_create_parent(path, 0755))
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", path) ;
    }

    state_pack(pack, sta) ;

    log_trace("write status file at: ", path) ;
    if (!openwritenclose_unsafe(path, pack, STATE_STATE_SIZE))
        return 0 ;

    return 1 ;
}
