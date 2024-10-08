/*
 * set_ownerhome.c
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
#include <sys/types.h>
#include <pwd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/utils.h>
#include <66/constants.h>

int set_ownerhome(stralloc *base,uid_t owner)
{
    log_flow() ;

    char const *user_home = 0 ;
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
    if (!user_home) return 0 ;

    if (!auto_stra(base,user_home, "/"))
        log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    return 1 ;
}

int set_ownerhome_stack_byuid(char *store, uid_t owner)
{
    log_flow() ;

    char const *user_home = 0 ;
    int e = errno ;
    struct passwd *st = getpwuid(owner) ;
    errno = 0 ;
    if (!st) {
        if (!errno) errno = ESRCH ;
        return 0 ;
    }
    user_home = st->pw_dir ;
    errno = e ;
    if (!user_home)
        return 0 ;

    if (strlen(user_home) + 1 > SS_MAX_PATH_LEN) {
        errno = ENAMETOOLONG ;
        return 0 ;
    }

    auto_strings(store, user_home, "/") ;

    return 1 ;
}

int set_ownerhome_stack(char *store)
{
    log_flow() ;

    return set_ownerhome_stack_byuid(store, getuid()) ;
}
