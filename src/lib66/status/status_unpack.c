/*
 * status_unpack.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>

#include <oblibs/types.h>
#include <oblibs/clock.h>

#include <66/status.h>

void status_unpack(char const *pack, service_status_t *st)
{
    st->version = (uint8_t)pack[0] ;
    st->state = (uint8_t)pack[1] ;
    st->result = (uint8_t)pack[2] ;
    st->who = (uint8_t)pack[3] ;
    u32_unpack_big(pack + 4, &st->pid) ;
    u32_unpack_big(pack + 8, &st->code) ;
    clock_unpack(pack + 12, &st->stamp) ;
    clock_unpack(pack + 24, &st->readystamp) ;
    clock_unpack(pack + 36, &st->window_start) ;
    st->ndeaths = (uint8_t)pack[48] ;
}
