/*
 * rc_unsupervise.c
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

#include <66/rc.h>

#include <string.h>

#include <oblibs/log.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/ssexec.h>
#include <66/state.h>
#include <66/resolve.h>
#include <66/db.h>
#include <66/utils.h>
#include <66/constants.h>

#include <s6-rc/s6rc-servicedir.h>

int rc_unsupervise(ssexec_t *info,genalloc *ga,char const *sig,char const *const *envp)
{
    log_flow() ;

    size_t i = 0 ;
    ss_resolve_t_ref pres ;
    stralloc sares = STRALLOC_ZERO ;
    ss_state_t sta = STATE_ZERO ;

    char prefix[info->treename.len + 2] ;
    memcpy(prefix,info->treename.s,info->treename.len) ;
    memcpy(prefix + info->treename.len,"-",1) ;
    prefix[info->treename.len + 1] = 0 ;

    char live[info->livetree.len + 1 + info->treename.len + 1 ];
    memcpy(live,info->livetree.s,info->livetree.len) ;
    live[info->livetree.len] = '/' ;
    memcpy(live + info->livetree.len + 1,info->treename.s,info->treename.len) ;
    live[info->livetree.len + 1 + info->treename.len] = 0 ;

    if (!db_switch_to(info,envp,SS_SWSRC))
    {
        log_warnu("switch ",info->treename.s," to source") ;
        goto err ;
    }

    if (!rc_send(info,ga,sig,envp))
    {
        log_warnu("stop services") ;
        goto err ;
    }

    for (; i < genalloc_len(ss_resolve_t,ga) ; i++)
    {
        char const *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
        char const *name = string + genalloc_s(ss_resolve_t,ga)[i].name ;
        log_trace("delete directory service: ",string + genalloc_s(ss_resolve_t,ga)[i].runat) ;
        s6rc_servicedir_unsupervise(live,prefix,name,0) ;
        if (rm_rf(string + genalloc_s(ss_resolve_t,ga)[i].runat) < 0) goto err ;
    }
    if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))
    {
        log_warnusys("set revolve pointer to source") ;
        goto err ;
    }
    for (i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++)
    {
        pres = &genalloc_s(ss_resolve_t,ga)[i] ;
        char const *string = pres->sa.s ;
        char const *name = string + pres->name  ;
        char const *state = string + pres->state ;
        // remove the resolve/state file if the service is disabled
        if (!pres->disen)
        {
            log_trace("Delete resolve file of: ",name) ;
            ss_resolve_rmfile(sares.s,name) ;
            log_trace("Delete state file of: ",name) ;
            ss_state_rmfile(state,name) ;
        }
        else
        {
            ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
            ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_TRUE) ;
    //      ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
            ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_FALSE) ;
            ss_state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
            log_trace("Write state file of: ",name) ;
            if (!ss_state_write(&sta,state,name))
            {
                log_warnusys("write state file of: ",name) ;
                goto err ;
            }
        }
        log_info("Unsupervised successfully: ",name) ;
    }
    stralloc_free(&sares) ;
    return 1 ;
    err:
        stralloc_free(&sares) ;
        return 0 ;
}
