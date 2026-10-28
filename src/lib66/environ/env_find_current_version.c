/*
 * env_find_current_version.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/environ.h>
#include <66/constants.h>

int env_find_current_version(strbuf *sa,char const *svconf)
{
    log_flow() ;

    size_t svconflen = strlen(svconf) ;
    struct stat st ;
    char tmp[svconflen + SS_SYM_VERSION_LEN + 1] ;

    auto_strings(tmp,svconf,SS_SYM_VERSION) ;

    /** symlink may no exist yet e.g first activation of the service; any
     * other failure is no answer */
    if (lstat(tmp,&st) == -1)
        return errno == ENOENT ? 0 : -1 ;

    {
        char lnk[SS_MAX_PATH + 1] ;
        ssize_t lnklen = readlink(tmp, lnk, sizeof(lnk) - 1) ;
        if (lnklen == -1)
            return -1 ;
        if (!strbuf_copyb(sa, lnk, lnklen))
            return -1 ;
    }

    if (!strbuf_terminate(sa))
        return -1 ;

    return 1 ;
}
