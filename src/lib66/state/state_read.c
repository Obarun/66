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

#include <skalibs/djbunix.h>

#include <66/state.h>

int state_read(ss_state_t *sta, char const *src, char const *name)
{
    log_flow() ;

    char pack[SS_STATE_SIZE] ;
    size_t srclen = strlen(src) ;
    size_t namelen = strlen(name) ;

    char tmp[srclen + 1 + namelen + 1] ;
    memcpy(tmp,src,srclen) ;
    tmp[srclen] = '/' ;
    memcpy(tmp + srclen + 1, name, namelen) ;
    tmp[srclen + 1 + namelen] = 0 ;

    if (openreadnclose(tmp, pack, SS_STATE_SIZE) < SS_STATE_SIZE) return 0 ;
    state_unpack(pack, sta) ;

    return 1 ;
}
