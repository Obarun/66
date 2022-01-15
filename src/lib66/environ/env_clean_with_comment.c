/*
 * env_clean_with_comment.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>

#include <66/environ.h>
#include <skalibs/stralloc.h>

int env_clean_with_comment(stralloc *sa)
{
    log_flow() ;

    ssize_t pos = 0, r ;
    char *end = 0, *start = 0 ;
    stralloc final = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;

    if (!sastr_split_string_in_nline(sa))
        log_warnu_return(LOG_EXIT_ZERO,"split environment value") ;

    for (; pos < sa->len ; pos += strlen(sa->s + pos) + 1)
    {
        tmp.len = 0 ;
        if (!sastr_clean_string(&tmp,sa->s + pos))
            log_warnu_return(LOG_EXIT_ZERO,"clean environment string") ;
        /** keep a empty line between key=value pair and a comment */
        r = get_len_until(tmp.s,'=') ;
        end = r < 0 ? "\n" : "\n\n" ;
        start = r < 0 ? "" : "\n" ;

        if (tmp.s[0] == '#')
        {
            if (!sastr_rebuild_in_oneline(&tmp))
                log_warnu_return(LOG_EXIT_ZERO,"rebuild environment string in one line") ;
        }
        else
        {
            if (!environ_rebuild_line(&tmp))
                log_warnu_return(LOG_EXIT_ZERO,"rebuild environment line") ;
        }
        if (!stralloc_0(&tmp) ||
        !auto_stra(&final,start,tmp.s,end))
            log_warn_return(LOG_EXIT_ZERO,"stralloc") ;
    }
    sa->len = 0 ;
    if (!auto_stra(sa,final.s))
        log_warnu_return(LOG_EXIT_ZERO,"store environment value") ;

    stralloc_free(&tmp) ;
    stralloc_free(&final) ;

    return 1 ;
}
