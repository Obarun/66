/*
 * backup_make_new.c
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

#include <sys/stat.h>
#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/enum.h>
#include <66/resolve.h>
#include <66/tree.h>
#include <66/db.h>

// force: 0->check, 1->remove and create
int backup_make_new(ssexec_t *info, unsigned int type)
{
    log_flow() ;

    int r ;

    size_t newsrc, newback, typelen ;
    char *ptype = NULL ;

    if (type == TYPE_CLASSIC) {

        ptype = SS_SVC ;
        typelen = SS_SVC_LEN ;

    } else {

        ptype = SS_DB ;
        typelen = SS_DB_LEN ;
    }

    char src[info->base.len + SS_SYSTEM_LEN + 1 + info->treename.len + SS_SVDIRS_LEN + SS_RESOLVE_LEN + 1] ;
    auto_strings(src, info->base.s, SS_SYSTEM, "/", info->treename.s, SS_SVDIRS, ptype)
    newsrc = info->base.len + SS_SYSTEM_LEN + 1 + info->treename.len + SS_SVDIRS_LEN ;

    char back[info->base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + info->treename.len + SS_RESOLVE_LEN + 1] ;
    auto_strings(back, info->base.s, SS_SYSTEM, SS_BACKUP, "/", info->treename.s, ptype) ;
    newback = info->base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + info->treename.len ;

    r = scan_mode(back,S_IFDIR) ;
    if (r || (r < 0)) {

        log_trace("rm directory: ", back) ;
        if (rm_rf(back) < 0)
            log_warnusys_return(LOG_EXIT_ZERO,"remove: ",back) ;

        r = 0 ;
    }

    if (!r) {

        log_trace("create directory: ", back) ;
        if (!dir_create(back,0755))
            log_warnusys_return(LOG_EXIT_ZERO,"create directory: ",back) ;
    }

    log_trace("copy: ",src," to: ", back) ;
    if (!hiercopy(src, back))
        log_warnusys_return(LOG_EXIT_ZERO,"copy: ",src," to ",back) ;

    memcpy(src + newsrc,SS_RESOLVE,SS_RESOLVE_LEN) ;
    src[newsrc + SS_RESOLVE_LEN] = 0 ;

    memcpy(back + newback,SS_RESOLVE,SS_RESOLVE_LEN) ;
    back[newback + SS_RESOLVE_LEN] = 0 ;

    r = scan_mode(back,S_IFDIR) ;
    if (r || (r < 0)) {

        log_trace("rm directory: ", back) ;
        if (rm_rf(back) < 0)
            log_warnusys_return(LOG_EXIT_ZERO,"remove: ",back) ;

        r = 0 ;
    }

    if (!r) {

        log_trace("create directory: ", back) ;
        if (!dir_create(back,0755))
            log_warnusys_return(LOG_EXIT_ZERO,"create directory: ",back) ;
    }

    log_trace("copy: ",src," to: ", back) ;
    if (!hiercopy(src, back))
        log_warnusys_return(LOG_EXIT_ZERO,"copy: ",src," to ",back) ;

    return 1 ;
}
