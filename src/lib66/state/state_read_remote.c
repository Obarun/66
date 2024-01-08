/*
 * state_read_remote.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/djbunix.h>

#include <66/state.h>
#include <66/constants.h>

int state_read_remote(ss_state_t *sta, char const *dst)
{
    log_flow() ;

    int r ;
    char pack[STATE_STATE_SIZE] ;
    char dir[strlen(dst) + 1 + SS_STATUS_LEN + 1];

    auto_strings(dir, dst, "/", SS_STATUS) ;

    r = openreadnclose(dir, pack, STATE_STATE_SIZE) ;
    if (r < STATE_STATE_SIZE || r < 0)
        return 0 ;

    state_unpack(pack, sta) ;

    return 1 ;
}
