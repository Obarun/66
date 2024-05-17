/*
 * regex_get_file_name.c
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

#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/mill.h>
#include <oblibs/stack.h>

#include <skalibs/stralloc.h>

/* 0 filename undefine
 * -1 system error
 * should return at least 2 meaning :: no file define*/
int regex_get_file_name(char *filename, char const *str)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    _init_stack_(stk, strlen(str) + 1) ;
    parse_mill_t MILL_GET_COLON = {
    .open = ':', .close = ':',
    .skip = " \t\r", .skiplen = 3,
    .forceclose = 1,
    .inner.debug = "get_colon" } ;

    r = mill_element(&stk, str, &MILL_GET_COLON, &pos) ;
    if (r == -1)
        log_dieu(LOG_EXIT_SYS, "get filename of line: ", str) ;

    if (!stack_close(&stk))
        log_die(LOG_EXIT_SYS, "stack overflow") ;

    auto_strings(filename, stk.s) ;

    return pos ;
}
