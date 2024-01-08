/*
 * parse_clean_list.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
    sa->len = 0 ;

    if (!auto_stra(sa, str))
        return 0 ;

    if (!sastr_clean_element(sa))
        return 0 ;

    return 1 ;
}
