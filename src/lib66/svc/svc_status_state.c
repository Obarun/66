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
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>

#include <s6/supervise.h>

#include <66/svc.h>

int svc_status_state(char const *dir, unsigned char *up, unsigned char *ready)
{
    log_flow() ;

    s6_svstatus_t st ;
    if (!s6_svstatus_read(dir, &st))
        return 0 ; // no status: the daemon has never been supervised here

    *up = (st.pid && !st.flagfinishing) ? 1 : 0 ;
    *ready = st.flagready ? 1 : 0 ;
    return 1 ;
}
