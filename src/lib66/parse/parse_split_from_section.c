/*
 * parse_split_from_section.c
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
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>
#include <oblibs/mill.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h> //UINT_FMT

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum.h>

static ssize_t parse_get_previous_element(stralloc *sa, size_t where)
{
    size_t n = sastr_nelement(sa) ;
    n-- ;
    if ((n - where) < n)
        return -1 ;

    return sastr_find_element_byid(sa,n - where) ;
}

/* *
 * For now the ability to comment an entire section when
 * commenting the section name is not available anymore
 * */
int parse_split_from_section(resolve_service_t *res, stralloc *secname, char *str, char const *svname)
{
    log_flow() ;

    int e = 0, r = 0, found = 0 ;

    key_all_t const *list = total_list ;
    stralloc sakey = STRALLOC_ZERO ;

    // cpos -> current, ipos -> idx pos, tpos -> temporary pos, end -> end the parse process
    size_t len = strlen(str), cpos = 0, ipos = 0, tpos = 0, end = 0 ;
    char tline[len + 1] ;
    char store[len + UINT_FMT + 1] ; // +6 be paranoid
    char *line ;

    // find the name of the current section
    ssize_t previous_sec = parse_get_previous_element(secname, 0) ;

    if (previous_sec == -1) {
        log_warn("get previous section") ;
        goto err ;
    }

    int id = get_enum_by_key(secname->s + previous_sec) ;

    if (id < 0) {
        log_warn("invalid section name: ", secname->s + previous_sec, " for service: ", svname) ;
        goto err ;
    }

    log_trace("parsing section: ", secname->s + previous_sec) ;

    if (!strcmp(secname->s + previous_sec, enum_str_section[SECTION_ENV])) {
        str[strlen(str)] = 0 ;

        if (!parse_store_g(res, str, SECTION_ENV, KEY_ENVIRON_ENVAL)) {
            log_warnu("store resolve file of: ", svname) ;
            goto err ;
        }

        goto end ;
    }

    while(cpos < len) {

        ipos = 0 ;
        tpos = 0 ;
        end = 0 ;
        sakey.len = 0 ;
        line = (char *)str + cpos ; // (char *) shut up compiler

        /** comment must be the first character found
         * at the begin of the line */
        if (line[tpos] == '#') {
            cpos += get_len_until(line, '\n') + 1 ;
            continue ;
        }

        // empty line or no more key=value?
        while (get_sep_before(line, '=', '\n') < 1){

                // Try to see if it's an empty line.
                r = get_len_until(line, '\n') ;
                if (r < 0) {
                    // end of string
                    end++ ;
                    break ;
                }

                cpos += r + 1 ; // +1 to remove '\n'

                if (cpos >= len) {
                    // end of string
                    end++ ;
                    break ;
                }

                line = (char *)str + cpos ;
        }

        if (end)
            break ;

        // get a cleaned key string
        wild_zero_all(&MILL_GET_KEY) ;
        r = mill_element(&sakey, line, &MILL_GET_KEY, &tpos) ;
        if (r < 1) {
            log_warnu("get key at frontend service file of service: ", svname, " from line: ", line, " -- please make a bug report") ;
            goto err ;
        } else if (!r || tpos + cpos >= len) {
                // '=' was not find or end of string?
                break ;
        }

        // copy the string to parse
        auto_strings(tline, line) ;

        tpos = 0 ;

        // loop around all keys from the section
        while (*total_list[id].list[ipos].name) {

            found = 0 ;

            // look for a valid key name
            if (*list[id].list[ipos].name && !strcmp(sakey.s, *list[id].list[ipos].name)) {

                found = 1 ;

                log_trace("parsing key: ", get_key_by_key_all(id, ipos)) ;

                switch(list[id].list[ipos].expected) {

                    case EXPECT_QUOTE:

                        r = get_len_until(tline, '\n') ;

                        if (r >= 0)
                            /** user migth forgot to close the double-quotes.
                             * so, only keep the line and not the complete string
                             * +1 for the parse_line_g which search for the '\n' character */
                            tline[r + 1] = 0 ;

                        wild_zero_all(&MILL_GET_DOUBLE_QUOTE) ;
                        if (!parse_line_g(store, &MILL_GET_DOUBLE_QUOTE, tline, &tpos))
                            parse_error_return(0, 6, id, ipos) ;

                        break ;

                    case EXPECT_BRACKET:

                        if (!parse_parentheses(store, tline, &tpos))
                            parse_error_return(0, 6, id, ipos) ;

                        break ;

                    case EXPECT_LINE:
                    case EXPECT_UINT:
                    case EXPECT_SLASH:

                        wild_zero_all(&MILL_GET_VALUE) ;
                        if (!parse_line_g(store, &MILL_GET_VALUE, tline, &tpos))
                            parse_error_return(0, 6, id, ipos) ;

                        break ;

                    default:
                        return 0 ;
                }
            }

            cpos += tpos ;

            if (found) {

                if (!parse_store_g(res, store, id, ipos)) {
                    log_warnu("store resolve file of: ", svname) ;
                    goto err ;
                } ;

                break ;
            }

            ipos++ ;
        }

        if (!found && r >= 0) {
            log_warn("unknown key: ", sakey.s," : in section: ",  secname->s + previous_sec, " -- ignoring it") ;
            tpos = get_len_until(line, '\n') ;
            cpos += tpos + 1 ;
        }
    }
    end:

    e = 1 ;

    err:
        stralloc_free(&sakey) ;
        return e ;
}
