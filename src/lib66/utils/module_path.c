/*
 * module_path.c
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

#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>

/* @sdir -> service dir
 * @mdir -> module dir */
int module_path(stralloc *sdir, stralloc *mdir, char const *sv,char const *frontend_src, uid_t owner)
{
    log_flow() ;

    int r, insta ;
    stralloc sainsta = STRALLOC_ZERO ;
    stralloc mhome = STRALLOC_ZERO ; // module user dir
    stralloc shome = STRALLOC_ZERO ; // service user dir
    char const *src = 0 ;
    char const *dest = 0 ;

    insta = instance_check(sv) ;
    instance_splitname(&sainsta,sv,insta,SS_INSTANCE_TEMPLATE) ;

    if (!owner)
    {
        src = SS_MODULE_ADMDIR ;
        dest = frontend_src ;
    }
    else
    {
        if (!set_ownerhome(&mhome,owner)) log_warnusys_return(LOG_EXIT_ZERO,"set home directory") ;
        if (!stralloc_cats(&mhome,SS_MODULE_USERDIR)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
        if (!stralloc_0(&mhome)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
        mhome.len-- ;
        src = mhome.s ;

        if (!set_ownerhome(&shome,owner)) log_warnusys_return(LOG_EXIT_ZERO,"set home directory") ;
        if (!stralloc_cats(&shome,SS_SERVICE_USERDIR)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
        if (!stralloc_0(&shome)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
        shome.len-- ;
        dest = shome.s ;

    }
    if (!auto_stra(mdir,src,sainsta.s)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
    r = scan_mode(mdir->s,S_IFDIR) ;
    if (!r || r == -1)
    {
        mdir->len = 0 ;
        src = SS_MODULE_ADMDIR ;
        if (!auto_stra(mdir,src,sainsta.s)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
        r = scan_mode(mdir->s,S_IFDIR) ;
        if (!r || r == -1)
        {
            mdir->len = 0 ;
            src = SS_MODULE_SYSDIR ;
            if (!auto_stra(mdir,src,sainsta.s)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
            r = scan_mode(mdir->s,S_IFDIR) ;
            if (!r || r == -1) log_warnu_return(LOG_EXIT_ZERO,"find module: ",sv) ;
        }

    }
    if (!auto_stra(sdir,dest,sv)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    stralloc_free(&sainsta) ;
    stralloc_free(&mhome) ;
    stralloc_free(&shome) ;
    return 1 ;
}
