/*
 * parse_value.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum_parser.h>

int parse_value(strbuf *store, lexer_config *kcfg, resolve_enum_table_t table)
{
    log_flow() ;

    size_t pos = 0 ;
    lexer_config vcfg = LEXER_CONFIG_ZERO ;
    _alloc_strbuf_(stk, kcfg->slen - kcfg->cpos + 1) ;
    uint32_t kid = table.u.parser.id ;
    key_description_t const *list = table.u.parser.list ;

    log_trace("parsing value of key: ", *list[kid].name) ;

    /* the value can be arbitrarily large (env, module contents, scripts): write
     * through the API so stk grows onto the heap instead of overflowing the seed. */
    if (!strbuf_copyb(&stk, kcfg->str + kcfg->cpos, kcfg->slen - kcfg->cpos) ||
        !strbuf_uncounted(&stk))
            parse_error_return(LOG_EXIT_ZERO, 6, table) ;

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

    if (!strbuf_terminate(store))
        log_warnu_return(LOG_EXIT_ZERO, "strbuf") ;
    store->len-- ;

    return 1 ;
}