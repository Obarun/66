/*
 * service_resolve_setlognwrite.c
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
#include <oblibs/string.h>

#include <66/enum.h>
#include <66/state.h>
#include <66/resolve.h>
#include <66/service.h>

int service_resolve_setlognwrite(resolve_service_t *sv, char const *dst)
{
    log_flow() ;

    if (!sv->logger) return 1 ;

    int e = 0 ;
    ss_state_t sta = STATE_ZERO ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    resolve_init(wres) ;

    char *str = sv->sa.s ;
    size_t svlen = strlen(str + sv->name) ;
    char descrip[svlen + 7 + 1] ;
    auto_strings(descrip, str + sv->name, " logger") ;

    size_t runlen = strlen(str + sv->runat) ;
    char live[runlen + 4 + 1] ;
    memcpy(live,str + sv->runat,runlen) ;
    if (sv->type >= TYPE_BUNDLE) {

        memcpy(live + runlen,"-log",4)  ;

    } else memcpy(live + runlen,"/log",4)  ;

    live[runlen + 4] = 0 ;

    res.type = sv->type ;
    res.name = resolve_add_string(wres,str + sv->logger) ;
    res.description = resolve_add_string(wres,str + sv->description) ;
    res.version = resolve_add_string(wres,str + sv->version) ;
    res.logreal = resolve_add_string(wres,str + sv->logreal) ;
    res.logassoc = resolve_add_string(wres,str + sv->name) ;
    res.dstlog = resolve_add_string(wres,str + sv->dstlog) ;
    res.live = resolve_add_string(wres,str + sv->live) ;
    res.runat = resolve_add_string(wres,live) ;
    res.tree = resolve_add_string(wres,str + sv->tree) ;
    res.treename = resolve_add_string(wres,str + sv->treename) ;
    res.state = resolve_add_string(wres,str + sv->state) ;
    res.src = resolve_add_string(wres,str + sv->src) ;
    res.down = sv->down ;
    res.disen = sv->disen ;

    if (sv->exec_log_run > 0)
        res.exec_log_run = resolve_add_string(wres,str + sv->exec_log_run) ;

    if (sv->real_exec_log_run > 0)
        res.real_exec_log_run = resolve_add_string(wres,str + sv->real_exec_log_run) ;

    if (state_check(str + sv->state,str + sv->logger)) {

        if (!state_read(&sta,str + sv->state,str + sv->logger)) {

            log_warnusys("read state file of: ",str + sv->logger) ;
            goto err ;
        }

        if (!sta.init)
            state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_TRUE) ;

        state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
        state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;

        if (!state_write(&sta,str + sv->state,str + sv->logger)) {
            log_warnusys("write state file of: ",str + sv->logger) ;
            goto err ;
        }
    }

    if (!resolve_write(wres,dst,res.sa.s + res.name)) {
        log_warnusys("write resolve file: ",dst,SS_RESOLVE,"/",res.sa.s + res.name) ;
        goto err ;
    }

    e = 1 ;

    err:
        resolve_free(wres) ;
        return e ;
}
