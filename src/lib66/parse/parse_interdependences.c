/*
 * parse_interdependences.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <unistd.h> // getuid

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/parse.h>
#include <66/ssexec.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/instance.h>
#include <66/module.h>

int parse_interdependences(char const *service, char const *list, unsigned int listlen, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint8_t force, uint8_t conf, char const *forced_directory, char const *main, char const *inns, char const *intree)
{
    log_flow() ;

    int r, e = 0 ;
    size_t pos = 0, len = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    uint8_t exlen = 3 ;
    char const *exclude[3] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1, SS_MODULE_CONFIG_DIR } ;

    if (listlen) {

        if (!sastr_clean_string(&sa, list)) {
            log_warnu("clean the string") ;
            goto freed ;
        }

        char t[sa.len + 1] ;

        sastr_to_char(t, &sa) ;

        len = sa.len ;

        for (; pos < len ; pos += strlen(t + pos) + 1) {

            sa.len = 0 ;
            char const *name = t + pos ;
            char ainsta[strlen(name) + 1] ;
            int insta = -1 ;

            log_trace("parse interdependences ", name, " of service: ", service) ;

            insta = instance_check(name) ;

            if (insta > 0) {

                if (!instance_splitname(&sa, name, insta, SS_INSTANCE_NAME))
                    log_die(LOG_EXIT_SYS, "split instance service of: ", name) ;

                auto_strings(ainsta, sa.s) ;
                sa.len = 0 ;
            }

            if (!strcmp(main, name))
                log_die(LOG_EXIT_USER, "direct cyclic interdependences detected -- ", main, " depends on: ", service, " which depends on: ", main) ;

            r = service_frontend_path(&sa, name, getuid(), forced_directory, exclude, exlen) ;
            if (r < 1) {
                log_warnu( "get frontend service file of: ", name) ;
                goto freed ;
            }

            if (!stralloc_0(&sa))
                log_die_nomem("stralloc") ;

            /** nothing to do with the exit code.
             * forced_directory == 0 means that the service
             * comes from an external directory of the module.
             * In this case don't associated it at the module. */
            parse_frontend(sa.s, ares, areslen, info, force, conf, forced_directory, main, !forced_directory ? 0 : inns, !forced_directory ? 0 : intree) ;
        }

    } else
        log_trace("no interdependences found for service: ", service) ;

    e = 1 ;

    freed:
        stralloc_free(&sa) ;
        return e ;
}
