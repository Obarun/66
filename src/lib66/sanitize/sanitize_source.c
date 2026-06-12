/*
 * sanitize_source.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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

#include <oblibs/strbuf.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <66/constants.h>
#include <66/sanitize.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/state.h>

void sanitize_source(char const *name, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    int r ;
    ssize_t logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;

    r = service_is_g(name, STATE_FLAGS_ISPARSED) ;
    if (r == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", name, " -- please a bug report") ;

    if (!r) {

        int argc = 3 ;
        int m = 0 ;
        char const *prog = PROG ;
        char const *newargv[argc] ;

        newargv[m++] = "parse" ;
        newargv[m++] = name ;
        newargv[m] = 0 ;

        PROG = "parse" ;
        if (opt_dispatch(m, newargv, &cmd_parse, info))
            log_dieu(LOG_EXIT_SYS, "parse service: ", name) ;
        PROG = prog ;

    } else if (logname < 0 && FLAGS_ISSET(flag, GRAPH_COLLECT_PARSE)) {

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

        if (sta.toparse == STATE_FLAGS_TRUE) {

            uint32_t opstree = info->opt_tree ;
            _alloc_strbuf_(stk, info->treename.len + 1) ;
            auto_strings(stk.s, info->treename.s) ;

            if (!info->opt_tree) {

                info->treename.len = 0 ;

                if (!auto_strbuf(&info->treename, res.sa.s + res.treename))
                    log_die_nomem("strbuf") ;

                info->opt_tree = 1 ;
            }

            newargv[m++] = "parse" ;
            newargv[m++] = "-f" ;
            newargv[m++] = name ;
            newargv[m] = 0 ;

            PROG = "parse" ;
            if (opt_dispatch(m, newargv, &cmd_parse, info))
                log_dieu(LOG_EXIT_SYS, "parse service: ", name) ;
            PROG = prog ;

            if (!opstree) {
                info->treename.len = 0 ;
                if (!auto_strbuf(&info->treename, stk.s))
                    log_die_nomem("strbuf") ;
                info->opt_tree = opstree ;
            }
        }
        resolve_free(wres) ;
    }
}

