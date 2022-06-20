/*
 * svc_unsupervise.c
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

#include <66/svc.h>

#include <string.h>

#include <oblibs/log.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/state.h>

int svc_unsupervise(ssexec_t *info, resolve_service_t *sv, unsigned int len)
{
    log_flow() ;

    unsigned int pos = 0 ;
    ss_state_t sta = STATE_ZERO ;
    resolve_service_t_ref pres = 0 ;

    if (!svc_send(info, sv, len, "-d"))
        log_warnu_return(LOG_EXIT_ZERO, "stop services") ;

    for (; pos < len ; pos++) {

        char const *string = sv[pos].sa.s ;
        int r = access(string, F_OK) ;
        if (!r) {
            log_trace("delete service directory: ",string + sv[pos].runat) ;
            if (rm_rf(string + sv[pos].runat) < 0)
                log_warnu_return(LOG_EXIT_ZERO, "delete: ", string + sv[pos].runat) ;
        }
    }

    for (pos = 0 ; pos < len ; pos++) {

        pres = &sv[pos] ;
        char const *string = pres->sa.s ;
        char const *name = string + pres->name  ;
        char const *state = string + pres->state ;
        size_t treenamelen = strlen(string + pres->treename) ;
        char res_s[info->base.len + SS_SYSTEM + 1 + treenamelen + SS_SVDIRS_LEN + 1] ;

        // remove the resolve/state file if the service is disabled
        if (!pres->disen) {

            auto_strings(res_s, info->base.s, SS_SYSTEM, "/", string + pres->treename, SS_SVDIRS) ;

            log_trace("Delete resolve file of: ", name) ;
            resolve_rmfile(res_s, name) ;

            int r = access(state, F_OK) ;
            if (!r) {
                log_trace("Delete state file of: ", name) ;
                state_rmfile(state, name) ;
            }

        } else if (!access(state, F_OK)) {

            /**
             * The svc_unsupervise can be called with service which was
             * never initialized. In this case the live state directory
             * doesn't exist and we don't want to create a fresh one.
             * So check first if the state directory exist before
             * passing through here
             * */

            state_setflag(&sta, SS_FLAGS_RELOAD, SS_FLAGS_FALSE) ;
            state_setflag(&sta, SS_FLAGS_INIT, SS_FLAGS_TRUE) ;
    //      state_setflag(&sta, SS_FLAGS_UNSUPERVISE, SS_FLAGS_FALSE) ;
            state_setflag(&sta, SS_FLAGS_STATE, SS_FLAGS_FALSE) ;
            state_setflag(&sta, SS_FLAGS_PID, SS_FLAGS_FALSE) ;

            log_trace("Write state file of: ", name) ;
            if (!state_write(&sta, state, name))
                log_warnu_return(LOG_EXIT_ZERO, "write state file of: ", name) ;

        }

        log_info("Unsupervised successfully: ", name) ;
    }

    return 1 ;
}

