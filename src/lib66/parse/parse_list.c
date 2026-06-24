/*
 * parse_list.c
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
#include <oblibs/strbuf.h>
#include <oblibs/lexer.h>

#include <66/parse.h>

int parse_list(strbuf *stk)
{
    log_flow() ;

    lexer_config cfg = LEXER_CONFIG_LIST ;
    _alloc_strbuf_(tmp, stk->len + 1) ;

    if (!strbuf_copy(&tmp, stk) || !strbuf_uncounted(&tmp))
        return 0 ;

    stk->len = 0 ;

    if (!lexer_trim_with_g(stk, tmp.s, &cfg))
        return 0 ;

    return 1 ;
}