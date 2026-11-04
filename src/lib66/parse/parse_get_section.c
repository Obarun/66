/*
 * parse_get_section.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum.h>
#include <66/enum_parser.h>

int parse_get_section(lexer_config *acfg, unsigned int *ncfg, char const *str, size_t len)
{
    log_flow() ;

    size_t pos = 0 ;

    while (pos < len) {

        size_t c, o = pos, eol = pos + str_search(str + pos, '\n') ;

        while (o < eol && (str[o] == ' ' || str[o] == '\t' || str[o] == '\r'))
            o++ ;

        pos = eol + 1 ;

        if (!parse_section_header(str, o, &c))
            continue ;

        char name[c - o] ;
        memcpy(name, str + o + 1, c - o - 1) ;
        name[c - o - 1] = 0 ;

        ssize_t id = key_to_enum(enum_list_parser_section, name) ;
        if (id < 0) {
            log_warn("invalid section name: ", name, " -- ignoring it") ;
            continue ;
        }

        if (!(*ncfg) && id != E_PARSER_SECTION_MAIN)
            log_warn_return(LOG_EXIT_ZERO, "invalid frontend file -- section [Main] must be set first") ;

        log_trace("found section: ", name) ;

        /** acfg holds at most one slot per section type; a frontend with
         * more sections than that (e.g. a duplicated section) would overrun
         * it. */
        if (*ncfg >= E_PARSER_SECTION_ENDOFKEY)
            log_warn_return(LOG_EXIT_ZERO, "too many sections in frontend file") ;

        lexer_config cfg = LEXER_CONFIG_ZERO ;
        cfg.str = str ;
        cfg.slen = len ;
        cfg.opos = o ;
        cfg.cpos = c ;
        cfg.pos = c + 1 ;
        cfg.found = 1 ;

        acfg[(*ncfg)++] = cfg ;
    }

    if (!(*ncfg))
        log_warn_return(LOG_EXIT_ZERO, "invalid frontend file -- no section found") ;

    return 1 ;
}
