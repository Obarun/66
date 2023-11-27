/*
 * service_cmp_basedir.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/service.h>

int service_cmp_basedir(char const *dir)
{
    log_flow() ;

    /** dir can be 0, so nothing to do */
    if (!dir) return 1 ;

    int e = 0 ;
    size_t len = strlen(dir) ;
    uid_t owner = MYUID ;
    stralloc home = STRALLOC_ZERO ;

    char system[len + 1] ;
    char adm[len + 1] ;
    char user[len + 1] ;

    if (owner)
    {
        if (!set_ownerhome(&home,owner)) { log_warnusys("set home directory") ; goto err ; }
        if (!auto_stra(&home,SS_SERVICE_USERDIR)) { log_warnsys("stralloc") ; goto err ; }
        auto_strings(user,dir) ;
        user[strlen(home.s)] = 0 ;
    }

    if (len < strlen(SS_SERVICE_SYSDIR))
        if (len < strlen(SS_SERVICE_ADMDIR))
            if (owner) {
                if (len < strlen(home.s))
                    goto err ;
            } else goto err ;

    auto_strings(system,dir) ;
    auto_strings(adm,dir) ;

    system[strlen(SS_SERVICE_SYSDIR)] = 0 ;
    adm[strlen(SS_SERVICE_ADMDIR)] = 0 ;

    if (strcmp(SS_SERVICE_SYSDIR,system))
        if (strcmp(SS_SERVICE_ADMDIR,adm))
            if (owner) {
                if (strcmp(home.s,user))
                    goto err ;
            } else goto err ;

    e = 1 ;

    err:
        stralloc_free(&home) ;
        return e ;
}
