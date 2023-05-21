/*
 * state_read.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/djbunix.h>

#include <66/state.h>
#include <66/constants.h>
#include <66/service.h>

int state_read(ss_state_t *sta, resolve_service_t *res)
{
    log_flow() ;

    int r ;
    char pack[STATE_STATE_SIZE] ;

    r = openreadnclose(res->sa.s + res->path.status, pack, STATE_STATE_SIZE) ;
    if (r < STATE_STATE_SIZE || r < 0)
        return 0 ;

    state_unpack(pack, sta) ;

    return 1 ;
}
