/*
 * state_write.c
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
#include <oblibs/directory.h>

#include <skalibs/djbunix.h>

#include <66/state.h>
#include <66/constants.h>
#include <66/service.h>

int state_write(ss_state_t *sta, resolve_service_t *res)
{
    log_flow() ;

    size_t len = strlen(res->sa.s + res->path.home) + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + 1 + strlen(res->sa.s + res->name) ;
    char pack[STATE_STATE_SIZE] ;
    char dir[len + SS_STATE_LEN + 1] ;

    auto_strings(dir, res->sa.s + res->path.home, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", res->sa.s + res->name, SS_STATE) ;

    if (access(dir, F_OK) < 0) {
        log_trace("create directory: ", dir) ;
        if (!dir_create_parent(dir, 0755))
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", dir) ;
    }

    state_pack(pack, sta) ;

    log_trace("write status file at: ", res->sa.s + res->path.status) ;
    if (!openwritenclose_unsafe(res->sa.s + res->path.status, pack, STATE_STATE_SIZE))
        return 0 ;

    return 1 ;
}
