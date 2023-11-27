/*
 * parse_contents.c
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

#include <skalibs/stralloc.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum.h>

int parse_contents(resolve_wrapper_t_ref wres, char const *contents, char const *svname)
{
    log_flow() ;

    size_t clen = strlen(contents), pos = 0, start = 0, len = 0 ;

    stralloc secname = STRALLOC_ZERO ;
    stralloc sadir = STRALLOC_ZERO ;

    int e = 0, found = 0 ;

    while (pos < clen) {

        found = 0 ;

        /** keep the starting point of the beginning
         * of the section search process */
        start = pos ;

        // get the section name
        found = parse_section(&secname, contents, &pos) ;

        if (found == -1)
            log_die_nomem("stralloc") ;

        // we are not able to find any section and we are at the start of the file
        if (!found && !start) {
            log_warn("invalid frontend file of: ", svname, " -- no section found") ;
            goto err ;
        }

        if (!start && strcmp(secname.s, enum_str_section[SECTION_MAIN])) {
            log_warn("invalid frontend file of: ", svname, " -- section [main] must be set first") ;
            goto err ;
        }

        // not more section, end of file. We copy the rest of the file
        if (!found) {

            len = clen - start ;

            char tmp[clen + 1] ;

            auto_strings(tmp, contents + start + 1) ;

            if (!parse_split_from_section((resolve_service_t *)wres->obj, &secname, tmp, svname))
                goto err ;

            break ;
        }
        else {

            /** again, keep the starting point of the begin
             * of the section search process */
            start = pos ;

            found = 0 ;

            /* search for the next section
             * we don't want to keep the section name to
             * avoid a double entry at the next pass of the loop */
            len = secname.len ;
            found = parse_section(&secname, contents, &pos) ;
            secname.len = len ;

            if (found == -1)
                log_die_nomem("stralloc") ;

            int r = get_rlen_until(contents, '\n', pos - 1) ;

            // found a new one
            if (found) {

                len = r - start ;

                char tmp[clen + 1] ;
                memcpy(tmp, contents + start + 1, len) ;
                tmp[len] = 0 ;

                if (!parse_split_from_section((resolve_service_t *)wres->obj, &secname, tmp, svname))
                    goto err ;

                // jump the contents of the section just parsed
                start += len ;
            }
            else {

                len = clen - start  ;

                char tmp[clen + 1] ;

                auto_strings(tmp, contents + start + 1) ;

                if (!parse_split_from_section((resolve_service_t *)wres->obj, &secname, tmp, svname))
                    goto err ;

                // end of file
                break ;
            }
            /** restart the next research from the end of the previous
             * copy A.K.A start of the next section just found previously */
            pos = start ;
        }
    }

    e = 1 ;

    err:
        stralloc_free(&secname) ;
        stralloc_free(&sadir) ;
        return e ;
}
