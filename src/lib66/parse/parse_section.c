/*
 * parse_section.c
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
#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum.h>

int parse_section_main(resolve_service_t *res, const char *str)
{
    return parse_section(res, str, list_section_main, SECTION_MAIN) ;
}

int parse_section_start(resolve_service_t *res, const char *str)
{
    return parse_section(res, str, list_section_startstop, SECTION_START) ;
}

int parse_section_stop(resolve_service_t *res, const char *str)
{
    return parse_section(res, str, list_section_startstop, SECTION_STOP) ;
}

int parse_section_logger(resolve_service_t *res, const char *str)
{
    return parse_section(res, str, list_section_logger, SECTION_LOG) ;
}

int parse_section_environment(resolve_service_t *res, const char *str)
{
    int r = get_len_until(str, '\n') ;
    if (r < 0)
        r = 0 ;
    r++ ;
    size_t len = strlen(str + r) ;
    _alloc_stk_(store, len + 1) ;
    if (!stack_copy_g(&store, str + r) ||
        !stack_close(&store))
        log_warnu_return(LOG_EXIT_ZERO, "stack overflow") ;

    if (!parse_store_g(res, &store, SECTION_ENV, list_section_environment, KEY_ENVIRON_ENVAL))
        log_warnu_return(LOG_EXIT_ZERO, "store resolve file of: ", res->sa.s + res->name) ;

    return 1 ;
}

int parse_section_regex(resolve_service_t *res, const char *str)
{
    return parse_section(res, str, list_section_regex, SECTION_REGEX) ;
}

int parse_section(resolve_service_t *res, char const *str, key_description_t const *list, const int sid)
{
    log_flow() ;

    int kid = -1 ; // key id
    char const *secname = enum_str_section[sid] ;
    lexer_config kcfg = LEXER_CONFIG_KEY ;

    kcfg.str = str ;
    kcfg.slen = strlen(str) ;

    _alloc_stk_(key, kcfg.slen + 1) ;

    log_trace("parsing section: ", *list_section[sid].name) ;

    while (kcfg.pos < kcfg.slen) {

        kcfg.found = 0 ;
        key.len = 0 ;
        lexer_reset(&kcfg) ;
        kcfg.opos = kcfg.cpos = 0 ;

        kid = parse_key(&key, &kcfg, list) ;

        if (kid < 0)
            log_warnu_return(LOG_EXIT_ZERO, "get keys in section: ", secname) ;

        if (kcfg.found) {

            _alloc_stk_(store, kcfg.slen + 1) ;

            if (!parse_value(&store, &kcfg, sid, list, kid))
                log_warnu_return(LOG_EXIT_ZERO, "get value of key: ", key.s) ;

            if (!parse_store_g(res, &store, sid, list, kid))
                log_warnu_return(LOG_EXIT_ZERO, "store resolve file of: ", res->sa.s + res->name) ;
        }
    }
    return 1 ;
}
