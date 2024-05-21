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

int parse_section(resolve_service_t *res, char const *str, size_t sid)
{
    log_flow() ;

    int kid = -1 ; // key id
    char const *secname = enum_str_section[sid] ;
    lexer_config kcfg = LEXER_CONFIG_KEY ;

    kcfg.str = str ;
    kcfg.slen = strlen(str) ;

    if (!strcmp(enum_str_section[sid], enum_str_section[SECTION_ENV])) {

        int r = get_len_until(str, '\n') ;
        if (r < 0)
            r = 0 ;
        r++ ;
        size_t len = strlen(str + r) ;
        _alloc_stk_(store, len + 1) ;
        if (!stack_copy_g(&store, str + r) ||
            !stack_close(&store))
            log_warnu_return(LOG_EXIT_ZERO, "stack overflow") ;

        if (!parse_store_g(res, &store, SECTION_ENV, KEY_ENVIRON_ENVAL))
            log_warnu_return(LOG_EXIT_ZERO, "store resolve file of: ", res->sa.s + res->name) ;

        return 1 ;
    }

    _alloc_stk_(key, kcfg.slen + 1) ;

    while (kcfg.pos < kcfg.slen) {

        kcfg.found = 0 ;
        key.len = 0 ;
        kid = parse_key(&key, &kcfg) ;
        if (kid < 0)
            log_warnu_return(LOG_EXIT_ZERO, "get keys in section: ", secname) ;

        if (kcfg.found) {

            _alloc_stk_(store, kcfg.slen + 1) ;

            if (!parse_value(&store, &kcfg, sid, kid))
                log_warnu_return(LOG_EXIT_ZERO, "get value of key: ", key.s) ;

            if (!parse_store_g(res, &store, sid, kid))
                log_warnu_return(LOG_EXIT_ZERO, "store resolve file of: ", res->sa.s + res->name) ;
        }
    }
    return 1 ;
}
