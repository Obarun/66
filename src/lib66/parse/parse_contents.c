/*
 * parse_contents.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum.h>

int parse_contents(resolve_service_t *res, char const *str)
{
    log_flow() ;

    size_t len = strlen(str) ;
    unsigned int ncfg = 0, n = 0 ;
    lexer_config acfg[SECTION_ENDOFKEY] ;
    memset(acfg, 0, sizeof(struct lexer_config_s) * SECTION_ENDOFKEY) ;

    log_trace("search for section of service: ", res->sa.s + res->name) ;
    if (!parse_get_section(acfg, &ncfg, str, len))
        log_warn_return(LOG_EXIT_ZERO,"get section");

    char tmp[len + 1] ;

    for (; n < ncfg ; n++) {

        /** get characters between two sections or section and EOF
         * and ensure to have a new line at the end of the string,
         * Parser will looking for '=\n' to get a key. */
        if (n + 1 >= ncfg) {
            /* End of file */
            memcpy(tmp, str + acfg[n].cpos + 1, len - (acfg[n].cpos + 1)) ;
            tmp[len - (acfg[n].cpos + 1)] = '\n' ;
            tmp[(len - (acfg[n].cpos + 1)) + 1] = 0 ;
        } else {
            /* Next section exist */
            memcpy(tmp, str + acfg[n].cpos + 1, acfg[n+1].opos - (acfg[n].cpos + 1)) ;
            tmp[acfg[n+1].opos - (acfg[n].cpos + 1)] = '\n' ;
            tmp[(acfg[n+1].opos - (acfg[n].cpos + 1)) + 1] = 0 ;
        }

        char secname[(acfg[n].cpos - (acfg[n].opos + 1)) + 1] ;
        memcpy(secname, str + acfg[n].opos + 1, acfg[n].cpos - (acfg[n].opos + 1)) ;
        secname[acfg[n].cpos - (acfg[n].opos + 1)] = 0 ;

        ssize_t id = get_enum_by_key(list_section, secname) ;
        if (id < 0)
            log_warnu_return(LOG_EXIT_ZERO, "get id of section: ", secname, " -- please make a bug report") ;

        switch (id) {

            case SECTION_MAIN:
                if (!parse_section_main(res, tmp))
                    log_warnu_return(LOG_EXIT_ZERO,"parse section: ", secname, " of service: ", res->sa.s + res->name) ;
                break ;
            case SECTION_START:
                if (!parse_section_start(res, tmp))
                    log_warnu_return(LOG_EXIT_ZERO,"parse section: ", secname, " of service: ", res->sa.s + res->name) ;
                break ;
            case SECTION_STOP:
                if (!parse_section_stop(res, tmp))
                    log_warnu_return(LOG_EXIT_ZERO,"parse section: ", secname, " of service: ", res->sa.s + res->name) ;
                break ;
            case SECTION_LOG:
                if (!parse_section_logger(res, tmp))
                    log_warnu_return(LOG_EXIT_ZERO,"parse section: ", secname, " of service: ", res->sa.s + res->name) ;
                break ;
            case SECTION_ENV:
                if (!parse_section_environment(res, tmp))
                    log_warnu_return(LOG_EXIT_ZERO,"parse section: ", secname, " of service: ", res->sa.s + res->name) ;
                break ;
            case SECTION_REGEX:
                if (!parse_section_regex(res, tmp))
                    log_warnu_return(LOG_EXIT_ZERO,"parse section: ", secname, " of service: ", res->sa.s + res->name) ;
                break;

            default:
                /* never happen*/
                break;
        }
    }

    return 1 ;
}
