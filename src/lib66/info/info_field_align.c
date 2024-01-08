/*
 * info_field_align.c
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

#include <66/info.h>

void info_field_align (char buf[][INFO_FIELD_MAXLEN],char fields[][INFO_FIELD_MAXLEN],wchar_t const field_suffix[],size_t buflen)
{
    log_flow() ;

    size_t a = 0, b = 0, maxlen = 0, wlen[buflen], len = INFO_FIELD_MAXLEN+nb_el(field_suffix) ;

    int maxcol = 0, wcol[buflen] ;

    wchar_t wbuf[buflen][len] ;

    for(a = 0; a < buflen; a++)
        for (b = 0; b < len; b++)
            wbuf[a][b] = 0 ;

    for(a = 0; a < buflen; a++)
    {
        wlen[a] = mbstowcs(wbuf[a], buf[a], strlen(buf[a]) + 1) ;
        wcol[a] = wcswidth(wbuf[a], wlen[a]) ;
        if(wcol[a] > maxcol) {
            maxcol = wcol[a] ;
        }
        if(wlen[a] > maxlen) {
            maxlen = wlen[a] ;
        }
    }

    for(a = 0; a < buflen; a++)
    {
        size_t padlen = maxcol - wcol[a] ;
        wmemset(wbuf[a] + wlen[a], L' ', padlen) ;
        wmemcpy(wbuf[a] + wlen[a] + padlen, field_suffix, nb_el(field_suffix)) ;
        wcstombs(fields[a], wbuf[a], sizeof(wbuf[a])) ;
    }
}
