/*
 * rc_unsupervise.c
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

int rc_unsupervise(ssexec_t *info, resolve_service_t *sv, unsigned int len)
{
    log_flow() ;

    unsigned int pos = 0, e = 0 ;
    resolve_service_t_ref pres ;
    ss_state_t sta = STATE_ZERO ;

    char prefix[info->treename.len + 2] ;
    auto_strings(prefix, info->treename.s, "-") ;

    char live[info->livetree.len + 1 + info->treename.len + 1 ];
    auto_strings(live, info->livetree.s, "/", info->treename.s) ;

    if (!db_switch_to(info, SS_SWSRC)) {
        log_warnu("switch ", info->treename.s, " to source") ;
        goto err ;
    }

    if (!rc_send(info, ga, sig, envp)) {
        log_warnu("stop services") ;
        goto err ;
    }

    for (; pos < len ; pos++) {

        pres = &sv[pos] ;
        char const *string = pres->sa.s ;
        char const *name = string + pres->name ;

        log_trace("delete directory service: ", string + pres->runat) ;
        s6rc_servicedir_unsupervise(live, prefix, name, 0) ;

        if (rm_rf(string + pres->runat) < 0)
            goto err ;
    }

    char res_s[info->base.len + SS_SYSTEM + 1 + info->treename.len + SS_SVDIRS_LEN + 1] ;
    auto_strings(res_s, info->base.s, SS_SYSTEM, "/", info->treename.s, SS_SVDIRS) ;

    for (pos = 0 ; pos < len ; pos++) {

        pres = &sv[pos] ;
        char const *string = pres->sa.s ;
        char const *name = string + pres->name  ;
        char const *state = string + pres->state ;
        // remove the resolve/state file if the service is disabled
        if (!pres->disen)
        {
            log_trace("Delete resolve file of: ", name) ;
            resolve_rmfile(res_s, name) ;
            log_trace("Delete state file of: ", name) ;
            state_rmfile(state, name) ;
        }
        else
        {
            state_setflag(&sta, SS_FLAGS_RELOAD, SS_FLAGS_FALSE) ;
            state_setflag(&sta, SS_FLAGS_INIT, SS_FLAGS_TRUE) ;
    //      state_setflag(&sta, SS_FLAGS_UNSUPERVISE, SS_FLAGS_FALSE) ;
            state_setflag(&sta, SS_FLAGS_STATE, SS_FLAGS_FALSE) ;
            state_setflag(&sta, SS_FLAGS_PID, SS_FLAGS_FALSE) ;
            log_trace("Write state file of: ",name) ;
            if (!state_write(&sta, state, name))
            {
                log_warnusys("write state file of: ", name) ;
                goto err ;
            }
        }
        log_info("Unsupervised successfully: ", name) ;
    }
    e = 1 ;

    err:
        return e ;
}
