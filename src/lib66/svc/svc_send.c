/*
 * svc_send.c
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

#include <oblibs/log.h>

#include <66/svc.h>
#include <66/resolve.h>
#include <66/ssexec.h>

int svc_send(char const *const *list, unsigned int nservice, char **sig, unsigned int siglen, ssexec_t *info)
{
    log_flow() ;

    int nargc = 2 + nservice + siglen ;
    char const *newargv[nargc] ;
    unsigned int m = 0 ;

    newargv[m++] = "66-svctl" ;
    for (; *sig ; sig++)
        newargv[m++] = *sig ;

    for (; *list ; list++)
        newargv[m++] = *list ;

    newargv[m++] = 0 ;

    if (ssexec_svctl(nargc, newargv, info))
        return 0 ;

    return 1 ;
}
