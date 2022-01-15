/*
 * env_find_current_version.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/environ.h>
#include <66/constants.h>

int env_find_current_version(stralloc *sa,char const *svconf)
{
    log_flow() ;

    size_t svconflen = strlen(svconf) ;
    struct stat st ;
    char tmp[svconflen + SS_SYM_VERSION_LEN + 1] ;

    auto_strings(tmp,svconf,SS_SYM_VERSION) ;

    /** symlink may no exist yet e.g first activation of the service */
    if (lstat(tmp,&st) == -1)
        return 0 ;

    if (sareadlink(sa,tmp) == -1)
        return -1 ;

    if (!stralloc_0(sa))
        log_warnusys_return(LOG_EXIT_ZERO,"stralloc") ;

    return 1 ;
}
