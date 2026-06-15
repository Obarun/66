/*
 * state_pack.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <oblibs/types.h>

#include <66/state.h>

void state_pack(char *pack, ss_state_t *sta)
{
    log_flow() ;

    u32_pack_big(pack, sta->toinit) ;
    u32_pack_big(pack + 4, sta->toreload) ;
    u32_pack_big(pack + 8, sta->torestart) ;
    u32_pack_big(pack + 12, sta->tounsupervise) ;
    u32_pack_big(pack + 16, sta->toparse) ;
    u32_pack_big(pack + 20, sta->isparsed) ;
    u32_pack_big(pack + 24, sta->issupervised) ;
    u32_pack_big(pack + 28, sta->isup) ;
}
