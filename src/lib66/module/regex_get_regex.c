/*
 * regex_get_regex.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

void regex_get_regex(char *regex, char const *str)
{
    log_flow() ;

    size_t len = strlen(str) ;
    int pos = get_len_until(str,'=') ;
    if (!pos || pos == -1)
        log_dieu(LOG_EXIT_SYS, "get regex string of line: ", str) ;
    pos++ ; // remove '='
    char tmp[len + 1] ;
    memcpy(tmp,str + pos,len-pos) ;
    tmp[len-pos] = 0 ;
    auto_strings(regex,tmp) ;
}
