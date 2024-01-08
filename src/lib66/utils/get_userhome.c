/*
 * get_userhome.c
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

#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <66/utils.h>

char const *get_userhome(uid_t myuid)
{
    log_flow() ;

    char const *user_home = NULL ;
    struct passwd *st = getpwuid(myuid) ;
    int e = errno ;
    errno = 0 ;
    if (!st)
    {
        if (!errno) errno = ESRCH ;
        return 0 ;
    }
    user_home = st->pw_dir ;

    if (!user_home) return 0 ;
    errno = e ;
    return user_home ;
}
