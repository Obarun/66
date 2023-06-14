/*
 * sanitize_source.c
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

#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <66/constants.h>
#include <66/sanitize.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/state.h>

void sanitize_source(char const *name, ssexec_t *info)
{
    log_flow() ;

    int r ;
    ssize_t logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
    char atree[SS_MAX_TREENAME + 1] ;

    r = service_is_g(atree, name, STATE_FLAGS_ISPARSED) ;
    if (r == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", name, " -- please a bug report") ;

    if (!r || r == STATE_FLAGS_FALSE) {

        int argc = 3 ;
        int m = 0 ;
        char const *prog = PROG ;
        char const *newargv[argc] ;

        newargv[m++] = "parse" ;
        newargv[m++] = name ;
        newargv[m++] = 0 ;

        PROG = "parse" ;
        if (ssexec_parse(argc, newargv, info))
            log_dieu(LOG_EXIT_SYS, "parse service: ", name) ;
        PROG = prog ;

    } else if (logname < 0) {

        int argc = 4 ;
        int m = 0 ;
        char const *prog = PROG ;
        char const *newargv[argc] ;

        ss_state_t sta = STATE_ZERO ;
        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

        /** We can come from ssexec_reconfigure, in
         * this case we need to use the tree defined previously */
        r = resolve_read_g(wres, info->base.s, name) ;
        if (r <= 0)
            log_dieu(LOG_EXIT_SYS, "read resolve file: ", name) ;

        if (!state_read(&sta, &res))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name) ;

        if (service_is(&sta, STATE_FLAGS_TOPARSE) == STATE_FLAGS_TRUE) {

            if (!info->opt_tree) {

                info->treename.len = 0 ;

                if (!auto_stra(&info->treename, res.sa.s + res.treename))
                    log_die_nomem("stralloc") ;

                info->opt_tree = 1 ;
            }

            newargv[m++] = "parse" ;
            newargv[m++] = "-f" ;
            newargv[m++] = name ;
            newargv[m++] = 0 ;

            PROG = "parse" ;
            if (ssexec_parse(argc, newargv, info))
                log_dieu(LOG_EXIT_SYS, "parse service: ", name) ;
            PROG = prog ;
        }
        resolve_free(wres) ;
    }
}

