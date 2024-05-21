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

    char const *copy ;
    size_t tlen = len + 1 ;

    _alloc_sa_(tmp) ;

    if (!auto_stra(&tmp,sasv->s)) return 0 ;

    copy = svname + tlen ;

    if (!sastr_replace_g(&tmp,regex,copy))
        log_warnu_return(LOG_EXIT_ZERO, "replace instance character for service: ",svname) ;

    sasv->len = 0 ;

    return auto_stra(sasv, tmp.s) ;
}
