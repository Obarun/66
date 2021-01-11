/*
 * hpr_shutdown.c
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * This file is a strict copy of hpr_shutdown.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <stdint.h>

#include <oblibs/log.h>

#include <skalibs/uint32.h>
#include <skalibs/tai.h>

#include <66/hpr.h>

int hpr_shutdown (char const *live, unsigned int what, tain_t const *when, unsigned int grace)
{
    log_flow() ;

    char pack[5 + TAIN_PACK] = { "Shpr"[what] } ;
    tain_pack(pack+1, when) ;
    uint32_pack_big(pack + 1 + TAIN_PACK, (uint32_t)grace) ;
    return hpr_send(live,pack, 5 + TAIN_PACK) ;
}
