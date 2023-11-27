/*
 * regex_get_replace.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

void regex_get_replace(char *replace, char const *str)
{
    log_flow() ;

    int pos = get_len_until(str,'=') ;
    if (!pos || pos == -1)
       log_dieu(LOG_EXIT_SYS,"replace string of line: ", str) ;
    char tmp[pos + 1] ;
    memcpy(tmp,str,pos) ;
    tmp[pos] = 0 ;
    auto_strings(replace, tmp) ;
}
