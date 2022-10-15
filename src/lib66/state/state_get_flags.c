/*
 * state_get_flags.c
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

int state_get_flags(char const *base, char const *name, int flags)
{
    log_flow() ;

    /** not parsed.
     * Return -1 to make a distinction between
     * file absent and flag == 0. */
    if (!state_check(base, name))
        return -1 ;

    ss_state_t sta = STATE_ZERO ;

    if (!state_read(&sta, base, name))
        // should not happen
        return -1 ;

    switch (flags)
    {
        case STATE_FLAGS_TOINIT: return sta.toinit ;
        case STATE_FLAGS_TORELOAD: return sta.toreload ;
        case STATE_FLAGS_TORESTART: return sta.torestart ;
        case STATE_FLAGS_TOUNSUPERVISE: return sta.tounsupervise ;
        case STATE_FLAGS_ISDOWNFILE: return sta.isdownfile ;
        case STATE_FLAGS_ISEARLIER: return sta.isearlier ;
        case STATE_FLAGS_ISENABLED: return sta.isenabled ;
        case STATE_FLAGS_ISPARSED: return sta.isparsed ;
        case STATE_FLAGS_ISSUPERVISED: return sta.issupervised ;
        case STATE_FLAGS_ISUP: return sta.isup ;
        default:
            // should never happen
            return -1 ;
    }

}
