/*
 * service_frontend_path.c
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

#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/service.h>

int service_frontend_path(stralloc *sasrc, char const *sv, uid_t owner, char const *directory_forced)
{
    log_flow() ;

    int r, e = 0 ;
    char const *src = 0 ;
    char home[SS_MAX_PATH_LEN + 1 + strlen(SS_USER_DIR) + 1] ;

    if (directory_forced) {

        if (!service_cmp_basedir(directory_forced))
            log_die(LOG_EXIT_USER, "invalid base service directory: ", directory_forced) ;

        src = directory_forced ;
        r = service_frontend_src(sasrc, sv, src) ;
        if (r == -1)
            log_dieusys(LOG_EXIT_SYS, "parse source directory: ", src) ;

        if (!r)
            goto freed ;

    } else {

        if (!owner)
            src = SS_SERVICE_ADMDIR ;
        else {

            if (!set_ownerhome_stack(home))
                log_dieusys(LOG_EXIT_SYS, "set home directory") ;

            auto_strings(home + strlen(home), SS_USER_DIR) ;

            src = home ;
        }

        r = service_frontend_src(sasrc, sv, src) ;
        if (r == -1)
            log_dieusys(LOG_EXIT_SYS, "parse source directory: ", src) ;

        if (!r) {

            src = SS_SERVICE_ADMDIR ;
            r = service_frontend_src(sasrc, sv, src) ;
            if (r == -1)
                log_dieusys(LOG_EXIT_SYS, "parse source directory: ", src) ;

            if (!r) {

                src = SS_SERVICE_SYSDIR ;
                r = service_frontend_src(sasrc, sv, src) ;
                if (r == -1)
                    log_dieusys(LOG_EXIT_SYS, "parse source directory: ", src) ;

                if (!r)
                    goto freed ;
            }
        }
    }
    e = 1 ;

    freed:
        return e ;
}
