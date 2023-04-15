/*
 * parser_line_g.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
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

#include <skalibs/stralloc.h>

#include <66/parse.h>

/* @Return 2 if bad format */
int parse_line_g(char *store, parse_mill_t *config, char const *str, size_t *pos)
{
    int r = 0, e = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    r = mill_element(&sa, str, config, pos) ;
    if (r <= 0 || !sa.len)
        goto err ;

    if (!stralloc_0(&sa))
        goto err ;

    if (sa.s[0] == ' ')
        goto err ;

    r = get_len_until(str, '\n') ;
    if (r < 1)
        r = 0 ;

    (*pos) = r + 1 ; // +1 remove '\n'

    e = 1 ;

    auto_strings(store, sa.s) ;

    err:
        stralloc_free(&sa) ;
        return e ;
}
