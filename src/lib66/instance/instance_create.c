/*
 * instance_create.c
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

#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int instance_create(stralloc *sasv,char const *svname, char const *regex, int len)
{
    log_flow() ;

    int e = 0 ;
    char const *copy ;
    size_t tlen = len + 1 ;

    stralloc tmp = STRALLOC_ZERO ;

    if (!auto_stra(&tmp,sasv->s)) goto err ;

    copy = svname + tlen ;

    if (!sastr_replace_all(&tmp,regex,copy)){
        log_warnu("replace instance character for service: ",svname) ;
        goto err ;
    }

    stralloc_0(&tmp) ;

    sasv->len = 0 ;

    if (!auto_stra(sasv, tmp.s))
        goto err ;

    e = 1 ;
    err:
        stralloc_free(&tmp) ;
        return e ;
}
