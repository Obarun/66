/*
 * rc_manage.c
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

#include <skalibs/tai.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/resolve.h>
#include <66/state.h>
#include <66/constants.h>
#include <66/db.h>

#include <s6-rc/s6rc-servicedir.h>

/**@Return 1 on success
 * @Return 2 on empty database
 * @Return 0 on fail */
int rc_manage(ssexec_t *info, resolve_service_t *sv, unsigned int len)
{
    log_flow() ;

    int r ;
    unsigned int pos = 0 ;
    ss_state_t sta = STATE_ZERO ;
    size_t newlen ;

    char prefix[info->treename.len + 2] ;
    auto_strings(prefix, info->treename.s, "-") ;

    char live[info->livetree.len + 1 + info->treename.len + 1 ];
    auto_strings(live, info->livetree.s, "/", info->treename.s) ;

    res_s[info->tree.len + SS_SVDIRS_LEN + SS_DB + 1 + info->treename.len + SS_SVDIRS_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;

    auto_strings(res_s, info->tree.s, SS_SVDIRS, SS_DB, "/", info->treename.s, SS_SVDIRS, "/") ;

    newlen = info->tree.len + SS_SVDIRS_LEN + SS_DB_LEN + 1 + info->treename.len + SS_SVDIRS_LEN + 1 ;

    for (; pos < len ; pos++) {

        char const *string = sv[pos].sa.s ;
        char const *name = string + sv[pos].name  ;
        char const *runat = string + sv[pos].runat ;
        int type = sv[pos].type ;

        //do not try to copy a bundle or oneshot, this is already done.
        if (type != TYPE_LONGRUN)
            continue ;

        auto_strings(res_s + newlen, name) ;
        if (!hiercopy(res_s,runat))
            log_warnusys_return(LOG_EXIT_ZERO, "copy: ", res_s, " to: ", runat) ;
    }

    /** do not really init the service if you come from backup,
     * s6-rc-update will do the manage proccess for us. If we pass through
     * here a double copy of the same service is made and s6-rc-update will fail. */
    r = db_find_compiled_state(info->livetree.s, info->treename.s) ;
    if (!r) {

        tain deadline ;
        if (info->timeout)
            tain_from_millisecs(&deadline, info->timeout) ;
        else deadline = tain_infinite_relative ;

        tain_now_g() ;
        tain_add_g(&deadline, &deadline) ;

        r = s6rc_servicedir_manage_g(live, prefix, &deadline) ;
        if (r == -1)
            log_warnusys_return(LOG_EXIT_ZERO, "supervise service directories at: ", live, "/servicedirs") ;
    }

    for (pos = 0 ; i < len ; pos++) {

        char const *string = sv[pos].sa.s ;
        char const *name = string + sv[pos].name  ;
        char const *state = string + sv[pos].state ;
        log_trace("Write state file of: ", name) ;
        if (!state_write(&sta, state, name))
            log_warnusys_return(LOG_EXIT_ZERO, "write state file of: ", name) ;

        log_info("Initialized successfully: ",name) ;
    }

    return 1 ;
}

