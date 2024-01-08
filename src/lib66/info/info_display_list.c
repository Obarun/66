/*
 * info_display_list.c
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

#include <stddef.h>

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/lolstdio.h>

#include <66/info.h>

void info_display_list(char const *field, stralloc *list)
{
    log_flow() ;

    size_t a = 0 , b, cols, padding = 0, slen = 0 ;

    unsigned short maxcols = info_getcols_fd(1) ;

    padding = info_length_from_wchar(field) + 1 ;

    cols = padding ;

    for (; a < list->len ; a += strlen(list->s + a) + 1)
    {
        char const *str = list->s + a ;
        slen = info_length_from_wchar(str) ;
        if((maxcols > padding) && (cols + slen + 2 >= maxcols))
        {
            cols = padding ;
            if (buffer_puts(buffer_1,"\n") == -1) goto err ;
            for(b = 1 ; b <= padding ; b++)
                if (buffer_puts(buffer_1," ") == -1) goto err ;
        }
        else if (cols != padding)
        {
            if (buffer_puts(buffer_1," ") == -1) goto err ;
            cols += 2 ;
        }
        if (!bprintf(buffer_1,"%s",str)) goto err ;
        cols += slen ;
    }
    if (buffer_puts(buffer_1,"\n") == -1) goto err ;

    return ;
    err:
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}
