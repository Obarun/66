/*
 * parse_section_header.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stddef.h>

#include <oblibs/log.h>

#include <66/parse.h>

int parse_section_header(char const *str, size_t o, size_t *c)
{
    log_flow() ;

    if (str[o] != '[' || str[o + 1] < 'A' || str[o + 1] > 'Z')
        return 0 ;

    o += 2 ;
    while (str[o] >= 'a' && str[o] <= 'z')
        o++ ;

    if (str[o] != ']')
        return 0 ;

    *c = o ;

    return 1 ;
}
