/*
 * instance_create.c
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

#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/utils.h>

int instance_create(stralloc *sasv,char const *svname, char const *regex, int len)
{
    log_flow() ;

    char const *copy ;
    size_t tlen = len + 1 ;

    stralloc tmp = STRALLOC_ZERO ;

    if (!auto_stra(&tmp,sasv->s)) goto err ;

    copy = svname + tlen ;

    if (!sastr_replace_all(&tmp,regex,copy)){
        log_warnu("replace instance character for service: ",svname) ;
        goto err ;
    }
    if (!stralloc_copy(sasv,&tmp)) goto err ;
    stralloc_free(&tmp) ;

    return 1 ;
    err:
        stralloc_free(&tmp) ;
        return 0 ;
}
