/*
 * parse_clean_line.c
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

#include <oblibs/mill.h>
#include <oblibs/string.h>
#include <oblibs/stack.h>
#include <oblibs/log.h>

#include <skalibs/stralloc.h>

int parse_clean_line(char *str)
{
    int r = 0 ;
    size_t tpos = 0 ;
    _init_stack_(stk, strlen(str) + 1) ;
    wild_zero_all(&MILL_CLEAN_LINE) ;
    r = mill_element(&stk, str, &MILL_CLEAN_LINE, &tpos) ;
    if (r <= 0)
        return 0 ;

    if (!stack_close(&stk))
        log_die(LOG_EXIT_SYS, "stack overflow") ;

    auto_strings(str, stk.s) ;

    return 1 ;
}
