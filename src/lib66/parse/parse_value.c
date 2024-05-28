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

#include <oblibs/log.h>
#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum.h>

int parse_value(stack *store, lexer_config *kcfg, const int sid, key_description_t const *list, const int kid)
{
    log_flow() ;

    size_t pos = 0 ;
    lexer_config vcfg = LEXER_CONFIG_ZERO ;

    log_trace("parsing value of key: ", *list[kid].name) ;

    switch(list[kid].expected) {

        case EXPECT_QUOTE:

            vcfg = LEXER_CONFIG_QUOTE ;
            lexer_reset(&vcfg) ;
            vcfg.str = kcfg->str + kcfg->cpos ;
            vcfg.slen = kcfg->slen - kcfg->cpos ;
            if (!lexer(store, &vcfg))
                parse_error_return(LOG_EXIT_ZERO, 6, sid, list, kid) ;

            kcfg->pos += vcfg.pos - 1 ;
            break ;

        case EXPECT_BRACKET:

            pos = 0 ;
            if (!parse_bracket(store, kcfg))
                parse_error_return(LOG_EXIT_ZERO, 6, sid, list, kid) ;
            kcfg->pos += pos  ;
            break ;

        case EXPECT_LINE:
        case EXPECT_UINT:
        case EXPECT_SLASH:

            vcfg = LEXER_CONFIG_INLINE ;
            lexer_reset(&vcfg) ;
            vcfg.str = kcfg->str + kcfg->cpos ;
            vcfg.slen = kcfg->slen - kcfg->cpos ;
            if (!lexer(store, &vcfg))
                parse_error_return(LOG_EXIT_ZERO, 6, sid, list, kid) ;
            kcfg->pos += vcfg.pos - 1 ;
            break ;
        default:
            return 0 ;
    }

    if (!stack_close(store))
        log_warnu_return(LOG_EXIT_ZERO, "stack overflow") ;

    return 1 ;
}