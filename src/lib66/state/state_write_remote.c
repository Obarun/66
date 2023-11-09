/*
 * state_write_remote.c
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

int state_write_remote(ss_state_t *sta, char const *dst)
{
    log_flow() ;

    size_t len = strlen(dst) ;
    char pack[STATE_STATE_SIZE] ;
    char dir[len + 1 + SS_STATUS_LEN + 1] ;

    auto_strings(dir, dst) ;

    if (access(dir, F_OK) < 0) {
        log_trace("create directory: ", dir) ;
        if (!dir_create_parent(dir, 0755))
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", dir) ;
    }

    state_pack(pack, sta) ;

    auto_strings(dir + len, "/", SS_STATUS) ;

    log_trace("write status file: ", dir) ;
    if (!openwritenclose_unsafe(dir, pack, STATE_STATE_SIZE))
        log_warnusys_return(LOG_EXIT_ZERO, "write status file: ", dir) ;

    return 1 ;
}
