/*
 * parser_parentheses.c
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
#include <stdint.h>

#include <oblibs/string.h>

#include <66/parse.h>

static char parse_char_next(char const *s, size_t slen, size_t *pos)
{
    char c = 0 ;
    if (*pos > slen) return -1 ;
    c = s[*pos] ;
    (*pos)++ ;
    return c ;
}

int parse_parentheses(char *store, char const *str, size_t *pos)
{
    int open = 0, close = 0, /* valid closed parentheses */ vp = 0, /* last valid closed parentheses position */ lvp = 0 ;
    uint8_t parentheses = 1 ;
    size_t o = 0, slen = strlen(str), e = 0 ;

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

    /** The general idea of this parsing process is to get
     * the same number of opened and closed parentheses.
     *
     * For intance, this function receive string starting with the key name field
     * at the very start of the string, '@execute=( ....'
     * we simply looking for corresponding closed parenthese and kept verbatim
     * of what we have been found between '(' and ')'.
    */
    while (parentheses && o < len) {

        char c = parse_char_next(line, len, &o) ;

        switch(c) {

            case '(':
                if (vp) {
                    vp-- ;
                    lvp = 0 ;
                }
                parentheses++ ;
                break ;

            case ')':
                {
                    if (vp) {
                        vp = 0 ;
                        lvp = 0 ;
                    }

                    if (parentheses - 1 == 0) {
                        /*
                         * This check the validity of the closed parenthese.
                         * For instance, a case...esac statement in scripts
                         * use a ')' without its corresponding reverse part.
                         *
                         * When examining the very first character of the next
                         * line and encountering either '#@' or '@', it determines
                         * the validity. If the validity check fails, it signifies
                         * that we remain within the script context*/

                        int /* last parenthese */ lp = o ;
                        e = 0 ;

                        e = get_len_until(line + o, '\n') + 1 ;
                        printf("e:%i - %i\n",o + e, parentheses) ;
                        if (o + e >= len) {
                            // end of string. this validate the parenthese
                            parentheses-- ;
                            o = lp ;
                            vp = 0 ;
                            break ;
                        }
                        o += e ;

                        /** with empty line we need to go futher and
                         * make this current check with the next line.*/
                        if (line[o] == '\n') {
                            o++ ;
                            vp = 1 ;
                            lvp = lp ;
                            break ;
                        }

                        /** Outside of the context specified (e.g., @execute=()),
                         * only '#' and '@' character combinaison are considered valid to
                         * validate the parentheses. If neither of these is present,
                         * it signifies that we are inside a script.*/
                        if (line[o] != '#' && line[o] != '@')
                            break ;

                        if (line[o] == '#') {

                            if (line[o + 1] == '@') {
                                /** a commented key validates the parenthese */
                                o = lp ;
                                parentheses-- ;
                                vp = 0 ;
                            } else {
                                // this is a comment
                                e = get_len_until(line + o, '\n') + 1 ;
                                o += e ;
                                vp = 1 ;
                                lvp = lp ;
                            }
                            break ;
                        }
                        o = lp ;
                        parentheses-- ;
                        vp = 0 ;
                        break ;

                    } else
                       parentheses-- ;

                    break ;
                }

            case '#':

                if (vp) {

                    /** we previously coming from a comment.
                     * this validates the parenthese.*/
                    if (line[o + 1] == '@') {
                        o = lvp ;
                        parentheses-- ;
                        vp = 0 ;
                    } else {
                        /** another comment, continue the check at the
                         * next line */
                        e = get_len_until(line + o, '\n') + 1 ;
                        o += e ;
                    }
                }
                break ;

            case '@':
                /** we previously coming from a comment.
                 * this validates the parenthese.*/
                if (vp) {
                    o = lvp ;
                    parentheses-- ;
                    vp = 0 ;
                }
                break ;
            case '\n':
                if (vp) {
                    /** we previously coming from an empty line.
                     * continue the check at the next line*/
                    e = get_len_until(line + o, '\n') + 1 ;
                    o += e ;
                }
                break ;
            case -1:
                return 0 ;
            default:
                if (vp) {
                    /** this invalidate the parentheses*/
                    vp = 0 ;
                }
                break ;
        }
    }

    if (vp && (o == len)) {
        /** end of string. this validate the parenthese */
        parentheses-- ;
        o = lvp ;
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
