/*
 * state_pack.c
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

#include <oblibs/log.h>

#include <skalibs/uint32.h>

#include <66/state.h>

void state_pack(char *pack, ss_state_t *sta)
{
    log_flow() ;

    uint32_pack_big(pack, sta->toinit) ;
    uint32_pack_big(pack + 4, sta->toreload) ;
    uint32_pack_big(pack + 8, sta->torestart) ;
    uint32_pack_big(pack + 12, sta->tounsupervise) ;
    uint32_pack_big(pack + 16, sta->toparse) ;
    uint32_pack_big(pack + 20, sta->isparsed) ;
    uint32_pack_big(pack + 24, sta->issupervised) ;
    uint32_pack_big(pack + 28, sta->isup) ;
}
