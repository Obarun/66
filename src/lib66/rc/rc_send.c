/*
 * rc_send.c
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

#include <66/rc.h>

#include <stddef.h>

#include <oblibs/log.h>

#include <skalibs/genalloc.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/ssexec.h>

int rc_send(ssexec_t *info, resolve_service_t *sv, unsigned int len, char const *sig)
{
    log_flow() ;

    unsigned int pos = 0 ;
    int nargc = 3 + len ;
    char const *newargv[nargc] ;
    unsigned int m = 0 ;

    newargv[m++] = "rc_send" ;
    newargv[m++] = sig ;

    for (; pos < len ; pos++)
        newargv[m++] = sv[pos].sa.s + sv[pos].name ;

    newargv[m++] = 0 ;

    if (ssexec_dbctl(nargc, newargv, (char const *const *) environ, info))
        return 0 ;

    return 1 ;
}
