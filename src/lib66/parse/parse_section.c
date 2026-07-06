/*
 * parse_section.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <oblibs/sbl.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum_parser.h>

int parse_section_main(resolve_service_t *res, const char *str)
{
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
    return parse_section(res, str, table) ;
}

int parse_section_start(resolve_service_t *res, const char *str)
{
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_START_ZERO ;
    return parse_section(res, str, table) ;
}

int parse_section_stop(resolve_service_t *res, const char *str)
{
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_STOP_ZERO ;
    return parse_section(res, str, table) ;
}

int parse_section_logger(resolve_service_t *res, const char *str)
{
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_LOGGER_ZERO ;
    return parse_section(res, str, table) ;
}

int parse_section_regex(resolve_service_t *res, const char *str)
{
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_REGEX_ZERO ;
    return parse_section(res, str, table) ;
}

int parse_section_execute(resolve_service_t *res, const char *str)
{
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_EXECUTE_ZERO ;
    return parse_section(res, str, table) ;
}

int parse_section(resolve_service_t *res, char const *str, resolve_enum_table_t table)
{
    log_flow() ;

    int kid = -1 ; // key id
    char const *secname = enum_str_parser_section[table.u.parser.sid] ;
    lexer_config kcfg = LEXER_CONFIG_KEY ;

    kcfg.str = str ;
    kcfg.slen = strlen(str) ;

    _alloc_sbl_(key, kcfg.slen + 1) ;

    log_trace("parsing section: ", secname) ;

    while (kcfg.pos < kcfg.slen) {

        kcfg.found = 0 ;
        key.len = 0 ;
        lexer_reset(&kcfg) ;
        kcfg.opos = kcfg.cpos = 0 ;

        kid = parse_key(&key, &kcfg, table) ;

        if (kid < 0)
            log_warnu_return(LOG_EXIT_ZERO, "get keys in section: ", secname) ;

        table.u.parser.id = kid ;

        if (kcfg.found) {

            _alloc_sbl_(store, kcfg.slen + 1) ;

            if (!parse_value(&store, &kcfg, table))
                log_warnu_return(LOG_EXIT_ZERO, "get value of key: ", key.s) ;

            if (!parse_store_g(res, &store, table))
                log_warnu_return(LOG_EXIT_ZERO, "store resolve file of: ", res->sa.s + res->name) ;
        }
    }
    return 1 ;
}
