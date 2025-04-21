/*
 * parse_get_value_of_key.c
 *
 * Copyright (c) 2024-2025 Eric Vidal <eric@obarun.org>
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
#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum.h>

int parse_get_value_of_key(stack *store, char const *str, resolve_enum_table_t table)
{
    log_flow() ;

    lexer_config cfg = LEXER_CONFIG_KEY ;
    int tkid = -1 ;
    uint32_t kid = table.u.parser.id ;
    cfg.str = str ;
    cfg.slen = strlen(str) ;
    _alloc_stk_(k, cfg.slen + 1) ;

    while (cfg.pos < cfg.slen) {

        k.len = 0 ;
        cfg.found = 0 ;
        tkid = parse_key(&k, &cfg, table) ;
        if (tkid < 0)
            log_warnu_return(LOG_EXIT_ZERO, "parse key: ", *table.u.parser.list[kid].name) ;

        if (cfg.found && tkid == kid) {

            if (!parse_value(store, &cfg, table))
                log_warnu_return(LOG_EXIT_ZERO, "get value of key: ", k.s) ;

            break ;
        }
    }

    return cfg.found ;
}