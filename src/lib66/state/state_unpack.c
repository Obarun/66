/*
 * state_unpack.c
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

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>

#include <66/state.h>

void state_unpack(char *pack,ss_state_t *sta)
{
    log_flow() ;

    uint32_t toinit ;
    uint32_t toreload ;
    uint32_t tounsupervise ;
    uint32_t issupervised ;

    u32_unpack_big(pack, &toinit) ;
    sta->toinit = toinit ;

    u32_unpack_big(pack + 4, &toreload) ;
    sta->toreload = toreload ;

    u32_unpack_big(pack + 8, &tounsupervise) ;
    sta->tounsupervise = tounsupervise ;

    u32_unpack_big(pack + 12, &issupervised) ;
    sta->issupervised = issupervised ;
}
