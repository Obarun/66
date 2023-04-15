/*
 * parser_parentheses.c
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
#include <stdint.h>

#include <oblibs/string.h>

#include <66/parse.h>

static char parse_char_next(char const *s, size_t *pos)
{
    char c = 0 ;
    size_t slen = strlen(s) ;
    if (*pos > slen) return -1 ;
    c = s[*pos] ;
    (*pos)++ ;
    return c ;
}

int parse_parentheses(char *store, char const *str, size_t *pos)
{
    int open = 0, close = 0 ;
    uint8_t parentheses = 1 ;
    size_t o = 0, slen = strlen(str) ;

    open = get_sep_before(str, '=', '(') ;
    if (open <= 0)
        return 0 ;
    close = get_sep_before(str, '(', ')') ;
    if (close < 0)
        return 0 ;
    open += get_len_until(str + open + 1, '(') ; // +1 remove '='
    open += 2 ; // +2 remove '('


    char line[slen + 1] ;
    auto_strings(line, str + open) ;
    size_t len = strlen(line) ;

    while(parentheses && o < len) {

        char c = parse_char_next(line, &o) ;

        switch(c) {

            case '(':
                parentheses++ ;
                break ;
            case ')':
                parentheses-- ;
                break ;
            case -1:
                return 0 ;
            default:
                break ;
        }
    }

    if (parentheses)
        return 0 ;

    (*pos) = open + len ;

    open = get_len_until(str, '\n') ;
    if (open < 1)
        open = 0 ;

    (*pos) = open + 1 ; // +1 remove '\n'

    len = len - (len - o) - 1 ;
    memcpy(store, line, len) ;
    store[len] = 0 ;

    return 1 ;
}
