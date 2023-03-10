/*
 * parse_dependencies.c
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
#include <unistd.h> // getuid

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/parser.h>
#include <66/ssexec.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/instance.h>

int parse_dependencies(resolve_service_t *res, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint8_t force, uint8_t conf, char const *forced_directory, char const *main, uint8_t requiredby)
{
    log_flow() ;

    uint32_t deps = !requiredby ? res->dependencies.ndepends : res->dependencies.nrequiredby ;
    uint32_t data = !requiredby ? res->dependencies.depends : res->dependencies.requiredby ;
    size_t pos = 0, len = 0 ;
    int r, e = 0 ;
    unsigned int residx = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    if (deps) {

        if (!sastr_clean_string(&sa, res->sa.s + data)) {
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

            log_trace("parse ", !requiredby ? "dependencies " : "requiredby ", name, " of service: ", res->sa.s + res->name) ;

            insta = instance_check(name) ;

            if (insta > 0) {

                if (!instance_splitname(&sa, name, insta, SS_INSTANCE_NAME))
                    log_die(LOG_EXIT_SYS, "split instance service of: ", name) ;

                auto_strings(ainsta, sa.s) ;
                sa.len = 0 ;
            }

            if (!strcmp(main, name))
                log_die(LOG_EXIT_USER, "direct cyclic dependencies detected -- ", main, " depends on: ", res->sa.s + res->name, " which depends on: ", main) ;


            r = service_frontend_path(&sa, name, getuid(), forced_directory) ;
            if (r < 1) {
                log_warnu( "get frontend service file of: ", name) ;
                goto freed ;
            }

            if (!stralloc_0(&sa))
                log_die_nomem("stralloc") ;

            /** nothing to do with the exit code */
            parse_frontend(sa.s, ares, areslen, info, force, conf, &residx, forced_directory, main) ;
        }

    } else
        log_trace("no ", !requiredby ? "dependencies" : "requiredby", " found for: ", res->sa.s + res->name) ;

    e = 1 ;

    freed:
        stralloc_free(&sa) ;
        return e ;
}
