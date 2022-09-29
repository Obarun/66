/*
 * parse_clean_line.c
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

#include <oblibs/mill.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

int parse_clean_line(char *str)
{

    int r = 0, e = 0 ;
    size_t tpos = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    wild_zero_all(&MILL_CLEAN_LINE) ;
    r = mill_element(&sa, str, &MILL_CLEAN_LINE, &tpos) ;
    if (r <= 0)
        goto err ;

    auto_strings(str, sa.s) ;

    e = 1 ;

    err:
        stralloc_free(&sa) ;
        return e ;

}
