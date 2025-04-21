/*
 * parse_value.c
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

#include <oblibs/log.h>
#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum_parser.h>

int parse_value(stack *store, lexer_config *kcfg, resolve_enum_table_t table)
{
    log_flow() ;

    size_t pos = 0 ;
    lexer_config vcfg = LEXER_CONFIG_ZERO ;
    _alloc_stk_(stk, kcfg->slen - kcfg->cpos) ;
    uint32_t kid = table.u.parser.id ;
    key_description_t const *list = table.u.parser.list ;

    log_trace("parsing value of key: ", *list[kid].name) ;
    memcpy(stk.s, kcfg->str + kcfg->cpos, strlen(kcfg->str + kcfg->cpos)) ;
    stk.s[strlen(kcfg->str + kcfg->cpos)] = 0 ;

    switch(list[kid].expected) {

        case E_PARSER_EXPECT_QUOTE:

            vcfg = LEXER_CONFIG_QUOTE ;
            lexer_reset(&vcfg) ;
            vcfg.str = stk.s ;
            vcfg.slen = kcfg->slen - kcfg->cpos ;
            if (!lexer(store, &vcfg))
                parse_error_return(LOG_EXIT_ZERO, 6, table) ;
            kcfg->pos += vcfg.pos - 1 ;
            break ;

        case E_PARSER_EXPECT_BRACKET:

            pos = 0 ;
            if (!parse_bracket(store, stk.s, table))
                parse_error_return(LOG_EXIT_ZERO, 6, table) ;
            kcfg->pos += pos  ;
            break ;

        case E_PARSER_EXPECT_LINE:
        case E_PARSER_EXPECT_UINT:
        case E_PARSER_EXPECT_SLASH:

            vcfg = LEXER_CONFIG_INLINE ;
            lexer_reset(&vcfg) ;
            vcfg.str = stk.s ;
            vcfg.slen = kcfg->slen - kcfg->cpos ;
            if (!lexer(store, &vcfg))
                parse_error_return(LOG_EXIT_ZERO, 6, table) ;
            kcfg->pos += vcfg.pos - 1 ;
            break ;
        default:
            return 0 ;
    }

    if (!stack_close(store))
        log_warnu_return(LOG_EXIT_ZERO, "stack overflow") ;

    return 1 ;
}