/*
 * parse_section.c
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

#include <string.h>

#include <oblibs/mill.h>
#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/parse.h>
#include <66/enum.h>

int parse_section(stralloc *secname, char const *str, size_t *pos)
{
    int id = -1 ;
    size_t len = strlen(str) ;
    size_t newpos = 0, found = 0 ;

    stralloc tmp = STRALLOC_ZERO ;

    while ((*pos) < len) {
        tmp.len = 0 ;
        newpos = 0 ;

        if (mill_element(&tmp, str + (*pos), &MILL_GET_SECTION_NAME, &newpos) == -1)
            goto end ;

        if (tmp.len) {
            if (!stralloc_0(&tmp))
               return -1 ;

            found = 1 ;

            // check the validity of the section name
            id = get_enum_by_key(tmp.s) ;

            if (id < 0) {
                log_warn("invalid section name: ", tmp.s, " -- ignoring it") ;
                newpos-- ; // " retrieve the last ']'"
                // find the start of the section and pass the next line
                id = get_len_until(str + (newpos - tmp.len), '\n') ;
                newpos = newpos - tmp.len + id + 1 ;
                found = 0 ;
            }
        }

        (*pos) += newpos ;

        if (found) break ;
    }

    if (found)
        if (!stralloc_catb(secname, tmp.s, strlen(tmp.s) + 1))
            return -1 ;

    end:
        stralloc_free(&tmp) ;
        return found ? 1 : 0 ;
}
