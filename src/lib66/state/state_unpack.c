/*
 * state_unpack.c
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

#include <skalibs/uint32.h>

#include <66/state.h>

void state_unpack(char *pack,ss_state_t *sta)
{
    log_flow() ;

    uint32_t toinit ;
    uint32_t toreload ;
    uint32_t torestart ;
    uint32_t tounsupervise ;
    uint32_t isdownfile ;
    uint32_t isearlier ;
    uint32_t isenabled ;
    uint32_t isparsed ;
    uint32_t issupervised ;
    uint32_t isup ;

    uint32_unpack_big(pack, &toinit) ;
    sta->toinit = toinit ;

    uint32_unpack_big(pack + 4, &toreload) ;
    sta->toreload = toreload ;

    uint32_unpack_big(pack + 8, &torestart) ;
    sta->torestart = torestart ;

    uint32_unpack_big(pack + 12, &tounsupervise) ;
    sta->tounsupervise = tounsupervise ;

    uint32_unpack_big(pack + 16, &isdownfile) ;
    sta->isdownfile = isdownfile ;

    uint32_unpack_big(pack + 20, &isearlier) ;
    sta->isearlier = isearlier ;

    uint32_unpack_big(pack + 24, &isenabled) ;
    sta->isenabled = isenabled ;

    uint32_unpack_big(pack + 28, &isparsed) ;
    sta->isparsed = isparsed ;

    uint32_unpack_big(pack + 32, &issupervised) ;
    sta->issupervised = issupervised ;

    uint32_unpack_big(pack + 36, &isup) ;
    sta->isup = isup ;
}
