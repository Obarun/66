/*
 * state_rmfile.c
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

#include <skalibs/posixplz.h>

#include <66/state.h>

void state_rmfile(char const *src,char const *name)
{
    log_flow() ;

    size_t srclen = strlen(src) ;
    size_t namelen = strlen(name) ;

    char tmp[srclen + 1 + namelen + 1] ;
    memcpy(tmp,src,srclen) ;
    tmp[srclen] = '/' ;
    memcpy(tmp + srclen + 1, name, namelen) ;
    tmp[srclen + 1 + namelen] = 0 ;

    unlink_void(tmp) ;
}
