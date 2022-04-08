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

    uint32_t reload ;
    uint32_t init ;
    uint32_t unsupervise ;
    uint32_t state ;
    uint64_t pid ;

    uint32_unpack_big(pack,&reload) ;
    sta->reload = reload ;
    uint32_unpack_big(pack+4,&init) ;
    sta->init = init ;
    uint32_unpack_big(pack+8,&unsupervise) ;
    sta->unsupervise = unsupervise ;
    uint32_unpack_big(pack+12,&state) ;
    sta->state = state ;
    uint64_unpack_big(pack+16,&pid) ;
    sta->pid = pid ;
}
