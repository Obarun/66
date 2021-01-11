/*
 * rc_send.c
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <66/rc.h>

#include <stddef.h>

#include <skalibs/genalloc.h>

#include <66/resolve.h>
#include <66/ssexec.h>

int rc_send(ssexec_t *info,genalloc *ga,char const *sig,char const *const *envp)
{
    log_flow() ;

    size_t i = 0 ;
    int nargc = 3 + genalloc_len(ss_resolve_t,ga) ;
    char const *newargv[nargc] ;
    unsigned int m = 0 ;

    newargv[m++] = "fake_name" ;
    newargv[m++] = sig ;

    for (; i < genalloc_len(ss_resolve_t,ga) ; i++)
        newargv[m++] = genalloc_s(ss_resolve_t,ga)[i].sa.s + genalloc_s(ss_resolve_t,ga)[i].name ;

    newargv[m++] = 0 ;

    if (ssexec_dbctl(nargc,newargv,envp,info))
        return 0 ;

    return 1 ;
}
