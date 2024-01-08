/*
 * yourgid.c
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

#include <sys/types.h>
#include <errno.h>
#include <pwd.h>

#include <oblibs/log.h>

#include <66/utils.h>

int yourgid(gid_t *passto,uid_t owner)
{
    log_flow() ;

    int e ;
    e = errno ;
    errno = 0 ;
    struct passwd *st ;
    st = getpwuid(owner) ;
    if (!st)
    {
        if (!errno) errno = ESRCH ;
        return 0 ;
    }
    *passto = st->pw_gid ;
    errno = e ;
    return 1 ;
}
