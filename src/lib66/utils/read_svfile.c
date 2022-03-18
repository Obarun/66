/*
 * read_svfile.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>

int read_svfile(stralloc *sasv,char const *name,char const *src)
{
    log_flow() ;

    int r ;
    size_t srclen = strlen(src) ;
    size_t namelen = strlen(name) ;

    char svtmp[srclen + namelen + 1] ;
    memcpy(svtmp,src,srclen) ;
    memcpy(svtmp + srclen, name, namelen) ;
    svtmp[srclen + namelen] = 0 ;

    size_t filesize=file_get_size(svtmp) ;
    if (!filesize)
        log_warn_return(LOG_EXIT_LESSONE,svtmp," is empty") ;

    r = openreadfileclose(svtmp,sasv,filesize) ;
    if(!r)
        log_warnusys_return(LOG_EXIT_ZERO,"open ", svtmp) ;

    /** ensure that we have an empty line at the end of the string*/
    if (!stralloc_cats(sasv,"\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
    if (!stralloc_0(sasv)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    return 1 ;
}
