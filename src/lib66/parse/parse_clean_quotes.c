/*
 * parse_clean_quotes.c
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

#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int parse_clean_quotes(char *str)
{
    int e = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    if (!auto_stra(&sa, str))
        goto err ;

    if (!sastr_clean_element(&sa))
        goto err ;

    if (!sastr_rebuild_in_oneline(&sa))
        goto err ;

    auto_strings(str, sa.s) ;

    e = 1 ;

    err:
        stralloc_free(&sa) ;
        return e ;
}
