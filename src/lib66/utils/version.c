/*
 * version.c
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

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/stack.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>

int version_compare(char const  *a, char const *b, uint8_t ndot)
{
    log_flow() ;

    int ar = 0 , br = 0, ap = 0 , bp = 0 ;
    uint8_t dot = 0 ;
    uint32_t anum, bnum ;
    size_t alen = strlen(a) ;
    size_t blen = strlen(b) ;

    while (dot < ndot + 1)
    {
        char one[alen + 1], two[blen + 1] ;
        auto_strings(one,a+ar) ;
        auto_strings(two,b+br) ;
        ap = get_len_until(a+ar,'.') ;
        bp = get_len_until(b+br,'.') ;
        one[ap] = 0 ;
        two[bp] = 0 ;
        ar += ++ap, br += ++bp ;
        if (!uint0_scan(one,&anum)) return -2 ;
        if (!uint0_scan(two,&bnum)) return -2 ;
        if (anum > bnum) return 1;
        if (anum < bnum) return -1 ;
        dot++ ;
    }
    return 0 ;
}

int version_store(stralloc *sa, char const *str, uint8_t ndot)
{
    log_flow() ;

    int r ;
    uint8_t dot = 0 ;
    uint32_t num ;
    size_t slen = strlen(str) ;
    char snum[slen + 1] ;
    auto_strings(snum,str) ;
    sa->len = 0 ;
    while(dot < ndot + 1)
    {
        size_t len = strlen(snum) ;
        r = get_rlen_until(snum,'.',len) ;
        if (r == -1 && dot != ndot) return 0 ;
        char tmp[len + 1] ;
        auto_strings(tmp,snum + r + 1) ;
        if (!uint0_scan(tmp,&num)) return 0 ;
        if (!stralloc_inserts(sa,0,tmp)) return -1 ;
        if (dot < ndot)
            if (!stralloc_inserts(sa,0,".")) return -1 ;
        dot++ ;
        snum[r] = 0 ;
    }
    if (!stralloc_0(sa)) return -1 ;
    sa->len-- ;
    return 1 ;
}
