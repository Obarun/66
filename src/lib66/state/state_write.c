/*
 * state_write.c
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

int state_write(ss_state_t *sta, char const *dst, char const *name)
{
    log_flow() ;

    char pack[SS_STATE_SIZE] ;
    size_t dstlen = strlen(dst) ;
    size_t namelen = strlen(name) ;

    char tmp[dstlen + 1 + namelen + 1] ;
    memcpy(tmp,dst,dstlen) ;
    tmp[dstlen] = '/' ;
    memcpy(tmp + dstlen + 1, name, namelen) ;
    tmp[dstlen + 1 + namelen] = 0 ;

    state_pack(pack,sta) ;
    if (!openwritenclose_unsafe(tmp,pack,SS_STATE_SIZE)) return 0 ;

    return 1 ;
}
