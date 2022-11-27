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
#include <66/state.h>

void sanitize_source(char const *name, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    int r, logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
    char atree[SS_MAX_TREENAME + 1] ;

    r = service_is_g(atree, name, STATE_FLAGS_ISPARSED) ;
    if (r == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", name, " -- please a bug report") ;

    if (!r) {

        int argc = 3 ;
        int m = 0 ;
        char *prog = PROG ;
        char const *newargv[argc] ;

        newargv[m++] = "parse" ;
        newargv[m++] = name ;
        newargv[m++] = 0 ;

        PROG = "parse" ;
        if (ssexec_parse(argc, newargv, info))
            log_dieu(LOG_EXIT_SYS, "parse service: ", name) ;
        PROG = prog ;

    } else if (FLAGS_ISSET(flag, STATE_FLAGS_TORELOAD) && logname < 0) {

        int argc = 4 ;
        int m = 0 ;
        char *prog = PROG ;
        char const *newargv[argc] ;

        newargv[m++] = "parse" ;
        newargv[m++] = "-f" ;
        newargv[m++] = name ;
        newargv[m++] = 0 ;

        PROG = "parse" ;
        if (ssexec_parse(argc, newargv, info))
            log_dieu(LOG_EXIT_SYS, "parse service: ", name) ;
        PROG = prog ;
    }

}

