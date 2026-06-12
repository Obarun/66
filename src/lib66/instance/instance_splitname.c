/*
 * instance_splitname.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <oblibs/strbuf.h>

#include <66/instance.h>

int instance_splitname(strbuf *sa,char const *name,int len,int what)
{
    log_flow() ;

    char const *copy ;
    size_t tlen = len + 1 ;

    char template[tlen + 1] ;
    memcpy(template,name,tlen) ;
    template[tlen] = 0 ;

    copy = name + tlen ;

    sa->len = 0 ;
    if (!what)
    {
        if (!strbuf_cats(sa,template) ||
        !strbuf_terminate(sa)) return 0 ;
    }
    else
    {
        if (!strbuf_catb(sa,copy,strlen(copy)) ||
        !strbuf_terminate(sa)) return 0 ;
    }
    return 1 ;
}
