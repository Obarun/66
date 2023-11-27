/*
 * instance_splitname_to_char.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

void instance_splitname_to_char(char *store, char const *name, int len, int what)
{
    log_flow() ;

    char const *copy ;
    size_t tlen = len + 1, clen = 0 ;

    char template[tlen + 1] ;
    memcpy(template,name,tlen) ;
    template[tlen] = 0 ;

    copy = name + tlen ;

    if (!what) {

        auto_strings(store, template) ;

    } else {

        clen = strlen(copy) ;
        memcpy(store, copy, clen) ;
        store[clen] = 0 ;
    }

}
