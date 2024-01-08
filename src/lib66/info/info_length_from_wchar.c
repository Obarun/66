/*
 * info_length_from_wchar.c
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
 * */

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <oblibs/log.h>

#include <66/info.h>

size_t info_length_from_wchar(char const *str)
{
    log_flow() ;

    ssize_t len ;
    wchar_t *wcstr ;
    if(!str || !str[0]) return 0 ;

    len = strlen(str) + 1 ;
    wcstr = calloc(len, sizeof(wchar_t)) ;
    len = mbstowcs(wcstr, str, len) ;
    len = wcswidth(wcstr, len) ;
    free(wcstr) ;

    return len == -1 ? 0 : (size_t)len  ;
}
