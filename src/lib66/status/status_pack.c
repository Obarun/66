/*
 * status_pack.c
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

void status_pack(char *pack, service_status_t const *st)
{
    pack[0] = (char)st->version ;
    pack[1] = (char)st->state ;
    pack[2] = (char)st->result ;
    pack[3] = (char)st->who ;
    u32_pack_big(pack + 4, st->pid) ;
    u32_pack_big(pack + 8, st->code) ;
    clock_pack(pack + 12, &st->stamp) ;
    clock_pack(pack + 24, &st->readystamp) ;
    clock_pack(pack + 36, &st->window_start) ;
    pack[48] = (char)st->ndeaths ;
}
