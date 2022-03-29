/*
 * service_frontend_path.c
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

#include <skalibs/stralloc.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/service.h>

int service_frontend_path(stralloc *sasrc,char const *sv, uid_t owner,char const *directory_forced)
{
    log_flow() ;

    int r ;
    char const *src = 0 ;
    int err = -1 ;
    stralloc home = STRALLOC_ZERO ;
    if (directory_forced)
    {
        if (!service_cmp_basedir(directory_forced)) { log_warn("invalid base service directory: ",directory_forced) ; goto err ; }
        src = directory_forced ;
        r = service_frontend_src(sasrc,sv,src) ;
        if (r == -1){ log_warnusys("parse source directory: ",src) ; goto err ; }
        if (!r) { log_warnu("find service: ",sv) ; err = 0 ; goto err ; }
    }
    else
    {
        if (!owner) src = SS_SERVICE_ADMDIR ;
        else
        {
            if (!set_ownerhome(&home,owner)) { log_warnusys("set home directory") ; goto err ; }
            if (!stralloc_cats(&home,SS_SERVICE_USERDIR)) { log_warnsys("stralloc") ; goto err ; }
            if (!stralloc_0(&home)) { log_warnsys("stralloc") ; goto err ; }
            home.len-- ;
            src = home.s ;
        }

        r = service_frontend_src(sasrc,sv,src) ;
        if (r == -1){ log_warnusys("parse source directory: ",src) ; goto err ; }
        if (!r)
        {
            src = SS_SERVICE_ADMDIR ;
            r = service_frontend_src(sasrc,sv,src) ;
            if (r == -1) { log_warnusys("parse source directory: ",src) ; goto err ; }
            if (!r)
            {
                src = SS_SERVICE_SYSDIR ;
                r = service_frontend_src(sasrc,sv,src) ;
                if (r == -1) { log_warnusys("parse source directory: ",src) ; goto err ; }
                if (!r) { log_warnu("find service: ",sv) ; err = 0 ; goto err ; }
            }
        }
    }
    stralloc_free(&home) ;
    return 1 ;
    err:
        stralloc_free(&home) ;
        return err ;
}
