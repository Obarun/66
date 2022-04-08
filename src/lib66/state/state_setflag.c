/*
 * state_setflag.c
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

#include <stdint.h>

#include <oblibs/log.h>

#include <66/state.h>

void state_setflag(ss_state_t *sta,int flags,int flags_val)
{
    log_flow() ;

    switch (flags)
    {
        case SS_FLAGS_RELOAD: sta->reload = flags_val ; break ;
        case SS_FLAGS_INIT: sta->init = flags_val ; break ;
        case SS_FLAGS_UNSUPERVISE: sta->unsupervise = flags_val ; break ;
        case SS_FLAGS_STATE: sta->state = flags_val ; break ;
        case SS_FLAGS_PID: sta->pid = (uint32_t)flags_val ; break ;
        default: return ;
    }
}
