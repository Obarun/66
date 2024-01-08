/*
 * state_set_flag.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

void state_set_flag(ss_state_t *sta, int flags, int flags_val)
{
    log_flow() ;

    switch (flags)
    {
        case STATE_FLAGS_TOINIT: sta->toinit = flags_val ; break ;
        case STATE_FLAGS_TORELOAD: sta->toreload = flags_val ; break ;
        case STATE_FLAGS_TORESTART: sta->torestart = flags_val ; break ;
        case STATE_FLAGS_TOUNSUPERVISE: sta->tounsupervise = flags_val ; break ;
        case STATE_FLAGS_TOPARSE: sta->toparse = flags_val ; break ;
        case STATE_FLAGS_ISPARSED: sta->isparsed = flags_val ; break ;
        case STATE_FLAGS_ISSUPERVISED: sta->issupervised = flags_val ; break ;
        case STATE_FLAGS_ISUP: sta->isup = flags_val ; break ;
        default: return ;
    }
}
