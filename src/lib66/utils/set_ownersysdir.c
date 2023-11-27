/*
 * set_ownersysdir.c
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

#include <pwd.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>

int set_ownersysdir(stralloc *base, uid_t owner)
{
    log_flow() ;

    char const *user_home = NULL ;
    int e = errno ;
    struct passwd *st = getpwuid(owner) ;
    errno = 0 ;
    if (!st)
    {
        if (!errno) errno = ESRCH ;
        return 0 ;
    }
    user_home = st->pw_dir ;
    errno = e ;
    if (user_home == NULL) return 0 ;

    if(owner > 0) {
        if (!auto_stra(base,user_home, "/", SS_USER_DIR))
            log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    } else {

        if (!auto_stra(base,SS_SYSTEM_DIR))
            log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
    }

    return 1 ;
}

int set_ownersysdir_stack(char *base, uid_t owner)
{
    log_flow() ;

    char const *user_home = NULL ;
    int e = errno ;
    struct passwd *st = getpwuid(owner) ;
    errno = 0 ;

    if (!st) {

        if (!errno) errno = ESRCH ;
        return 0 ;
    }

    user_home = st->pw_dir ;

    errno = e ;

    if (user_home == NULL)
        return 0 ;

    if (strlen(user_home) + 1 + strlen(SS_USER_DIR) > SS_MAX_PATH_LEN) {
        errno = ENAMETOOLONG ;
        return 0 ;
    }

    if(owner > 0)
        auto_strings(base, user_home, "/", SS_USER_DIR) ;
    else
        auto_strings(base, SS_SYSTEM_DIR) ;

    return 1 ;
}
