/*
 * svc_status_state.c
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

#include <string.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/svc.h>
#include <66/status.h>
#include <66/constants.h>

int svc_status_state(char const *dir, unsigned char *up, unsigned char *ready)
{
    log_flow() ;

    char file[strlen(dir) + SS_SUPERVISEDIR_LEN + 1 + SS_STATUS_LEN + 1] ;
    auto_strings(file, dir, SS_SUPERVISEDIR, "/", SS_STATUS) ;

    if (access(file, F_OK) < 0)
        return 0 ; // no status: the daemon has never been supervised here

    service_status_t st = STATUS_ZERO ;
    if (status_read(&st, file) < 0)
        return 0 ;

    *up = (st.state == STATUS_STATE_STARTING
        || st.state == STATUS_STATE_UP
        || st.state == STATUS_STATE_STOPPING) ? 1 : 0 ;
    *ready = st.state == STATUS_STATE_UP ? 1 : 0 ;
    return 1 ;
}
