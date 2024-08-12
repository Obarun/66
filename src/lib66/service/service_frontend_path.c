/*
 * service_frontend_path.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/service.h>

static void compute_exclude(char const **nexclude, char const **oexclude, uint8_t exlen, uid_t owner)
{
    log_flow() ;

    uint8_t pos = 0 ;

    if (exlen) {
        for (; pos < exlen ; pos++)
            nexclude[pos] = oexclude[pos] ;

        if (owner)
            nexclude[pos] = 0 ;
    } else
        nexclude[0] = 0 ;

    if (!owner) {
        if (!exlen) {
            nexclude[0] = "user" ;
            nexclude[1] = 0 ;
        } else {
            nexclude[pos++] = "user" ;
            nexclude[pos] = 0 ;
        }
    }
}

int service_frontend_path(stralloc *sasrc, char const *sv, uid_t owner, char const *directory_forced, char const **exclude, uint8_t exlen)
{
    log_flow() ;

    int r, e = 0 ;
    char const *src = 0 ;
    char home[SS_MAX_PATH_LEN + 1 + strlen(SS_SERVICE_USERDIR) + 1] ;
    char const *ex[exlen + 2] ;

    compute_exclude(ex, exclude, exlen, owner) ;

    if (directory_forced) {

        char const *exclude[1] = { 0 } ;

        //if (!service_cmp_basedir(directory_forced))
        //    log_die(LOG_EXIT_USER, "invalid base service directory: ", directory_forced) ;

        src = directory_forced ;
        r = service_frontend_src(sasrc, sv, src, exclude) ;
        if (r == -1)
            log_dieusys(LOG_EXIT_SYS, "parse source directory: ", src) ;

        if (!r)
            goto freed ;

    } else {

        if (!owner) {
            src = SS_SERVICE_ADMDIR ;
        } else {

            if (!set_ownerhome_stack(home))
                log_dieusys(LOG_EXIT_SYS, "set home directory") ;

            auto_strings(home + strlen(home), SS_SERVICE_USERDIR) ;

            src = home ;
        }

        r = service_frontend_src(sasrc, sv, src, ex) ;
        if (r == -1)
            log_dieusys(LOG_EXIT_SYS, "parse source directory: ", src) ;

        if (!r) {

            if (!owner) {
                goto next ;
            } else {
                src = SS_SERVICE_ADMDIR_USER ;
            }

            r = service_frontend_src(sasrc, sv, src, ex) ;
            if (r == -1)
                log_dieusys(LOG_EXIT_SYS, "parse source directory: ", src) ;

            next:
            if (!r) {

                if (!owner) {
                    src = SS_SERVICE_SYSDIR ;
                } else {
                    src = SS_SERVICE_SYSDIR_USER ;
                }

                r = service_frontend_src(sasrc, sv, src, ex) ;
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
