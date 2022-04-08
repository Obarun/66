/*
 * state_check_flags.c
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

#include <oblibs/log.h>

#include <66/state.h>

int state_check_flags(char const *src, char const *name,int flags)
{
    log_flow() ;

    /** unitialized at all, all flags == 0.
     * Return -1 to make a distinction between
     * file absent and flag == 0. */
    if (!state_check(src,name))
        return -1 ;

    ss_state_t sta = STATE_ZERO ;

    if (!state_read(&sta,src,name))
        // should not happen
        return -1 ;

    switch (flags)
    {
        case SS_FLAGS_RELOAD: return sta.reload ;
        case SS_FLAGS_INIT: return sta.init ;
        case SS_FLAGS_UNSUPERVISE: return sta.unsupervise ;
        case SS_FLAGS_STATE: return sta.state ;
        case SS_FLAGS_PID: return sta.pid ;
        default:
            // should never happen
            return -1 ;
    }

}
