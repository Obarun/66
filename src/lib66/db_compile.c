/*
 * db_compile.c
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

#include <66/db.h>

#include <s6-rc/config.h>//S6RC_BINPREFIX

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>

#include <skalibs/types.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/constants.h>

int db_compile(char const *workdir, char const *tree, char const *treename, char const *const *envp)
{
    int wstat, r ;
    pid_t pid ;
    size_t wlen = strlen(workdir), treelen = strlen(treename) ;

    char dest[wlen + SS_DB_LEN + 1 + treelen + 1] ;
    memcpy(dest,workdir,wlen) ;
    memcpy(dest + wlen,SS_DB,SS_DB_LEN) ;
    dest[wlen + SS_DB_LEN] = '/' ;
    memcpy(dest + wlen + SS_DB_LEN + 1,treename,treelen) ;
    dest[wlen + SS_DB_LEN + 1 + treelen] = 0 ;

    char src[wlen + SS_DB_LEN + SS_SRC_LEN + 1] ;
    memcpy(src,workdir,wlen) ;
    memcpy(src + wlen,SS_DB,SS_DB_LEN) ;
    memcpy(src + wlen + SS_DB_LEN,SS_SRC,SS_SRC_LEN) ;
    src[wlen + SS_DB_LEN + SS_SRC_LEN] = 0 ;

    r = scan_mode(dest,S_IFDIR) ;
    if (r)
    {
        if (rm_rf(dest) < 0)
            log_warnusys_return(LOG_EXIT_ZERO,"remove: ", dest) ;
    }

    char const *newargv[7] ;
    unsigned int m = 0 ;
    char fmt[UINT_FMT] ;
    fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;

    newargv[m++] = S6RC_BINPREFIX "s6-rc-compile" ;
    newargv[m++] = "-v" ;
    newargv[m++] = fmt ;
    newargv[m++] = "--" ;
    newargv[m++] = dest ;
    newargv[m++] = src ;
    newargv[m++] = 0 ;

    pid = child_spawn0(newargv[0],newargv,envp) ;
    if (waitpid_nointr(pid,&wstat, 0) < 0)
        log_warnusys_return(LOG_EXIT_ZERO,"wait for: ",newargv[0]) ;

    if (wstat)
        log_warnu_return(LOG_EXIT_ZERO,"compile: ",dest) ;

    return 1 ;
}
