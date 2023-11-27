/*
 * parser_line_g.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>

#include <oblibs/string.h>
#include <oblibs/mill.h>
#include <oblibs/stack.h>

#include <skalibs/stralloc.h>

#include <66/parse.h>

int parse_line_g(char *store, parse_mill_t *config, char const *str, size_t *pos)
{
    int r = 0 ;
    _init_stack_(stk, strlen(str) + 1) ;

    r = mill_element(&stk, str, config, pos) ;
    if (r <= 0 || !stk.len)
        return 0 ;

    if (!stack_close(&stk))
        return 0 ;

    if (stk.s[0] == ' ')
        return 0 ;

    r = get_len_until(str, '\n') ;
    if (r < 1)
        r = 0 ;

    (*pos) = r + 1 ; // +1 remove '\n'

    auto_strings(store, stk.s) ;

    return 1 ;
}
