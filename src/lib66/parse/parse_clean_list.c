/*
 * parse_clean_list.c
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

#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

int parse_clean_list(stralloc *sa, char const *str)
{
    int e = 0 ;
    sa->len = 0 ;

    if (!auto_stra(sa, str))
        goto err ;

    if (!sastr_clean_element(sa))
        goto err ;

    e = 1 ;

    err:
        return e ;
}
