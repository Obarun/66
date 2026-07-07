/*
 * parse_validator.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum.h>
#include <66/enum_parser.h>

int parse_validator_init(parse_validator_t *v, char const *frontend)
{
    log_flow() ;

    size_t len = strlen(frontend) ;
    unsigned int ncfg = 0, n = 0 ;
    lexer_config acfg[E_PARSER_SECTION_ENDOFKEY] ;

    memset(v, 0, sizeof(*v)) ;
    v->frontend = frontend ;

    memset(acfg, 0, sizeof(acfg)) ;

    if (!parse_get_section(acfg, &ncfg, frontend, len))
        log_warnu_return(LOG_EXIT_ZERO, "get section") ;

    for (; n < ncfg ; n++) {

        size_t cstart = acfg[n].cpos + 1 ;
        size_t cend = (n + 1 < ncfg) ? acfg[n + 1].opos : len ;

        size_t nlen = acfg[n].cpos - (acfg[n].opos + 1) ;
        char secname[nlen + 1] ;
        memcpy(secname, frontend + acfg[n].opos + 1, nlen) ;
        secname[nlen] = 0 ;

        ssize_t id = key_to_enum(enum_list_parser_section, secname) ;
        if (id < 0)
            log_warnu_return(LOG_EXIT_ZERO, "get id of section: ", secname, " -- please make a bug report") ;

        v->off[id] = cstart ;
        v->len[id] = cend - cstart ;
        v->present[id] = 1 ;
    }

    return 1 ;
}
