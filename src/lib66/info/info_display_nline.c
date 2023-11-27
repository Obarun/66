/*
 * info_display_nline.c
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
 * */

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/lolstdio.h>

#include <66/info.h>

void info_display_nline(char const *field,char const *str)
{
    log_flow() ;

    size_t pos = 0, padding = info_length_from_wchar(field) + 1, len ;

    stralloc sa = STRALLOC_ZERO ;

    if (!auto_stra(&sa, str))
        log_die_nomem("stralloc") ;

    if (!sastr_split_string_in_nline(&sa))
        log_dieu(LOG_EXIT_SYS,"split string in nline") ;

    len = sa.len ;
    char tmp[sa.len + 1] ;
    sastr_to_char(tmp, &sa) ;

    for (;pos < len ; pos += strlen(tmp + pos) + 1) {

        sa.len = 0 ;

        if (!auto_stra(&sa,tmp + pos))
            log_die_nomem("stralloc") ;

        if (field) {
            if (pos) {
                if (!bprintf(buffer_1,"%*s",padding,""))
                    log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
            }
        }
        info_display_list(field,&sa) ;
    }
    stralloc_free(&sa) ;
}
