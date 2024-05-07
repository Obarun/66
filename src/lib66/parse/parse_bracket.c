/*
 * parse_bracket.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum.h>

static char parse_char_next(char const *s, size_t slen, size_t *pos)
{
    char c = 0 ;
    if (*pos > slen) return -1 ;
    c = s[*pos] ;
    (*pos)++ ;
    return c ;
}

int parse_bracket(stack *store, lexer_config *kcfg)
{
    log_flow() ;

    int /* valid closed bracket */ vp = 1, /* last valid closed bracket position */ lvp = 0 ;
    uint8_t bracket = 1 ;
    size_t o = 0, e = 0 ;

    lexer_config cfg = LEXER_CONFIG_ZERO ;

    cfg.str = kcfg->str + kcfg->cpos ;
    cfg.slen = kcfg->slen - kcfg->cpos ;
    cfg.open = "(";
    cfg.olen = 1 ;
    cfg.close = ")" ;
    cfg.clen = 1 ;
    cfg.kopen = 0 ;
    cfg.kclose = 0 ;

    if (!lexer(store, &cfg))
        return 0 ;

    if (!cfg.found)
        return 0 ;

    char const *line =  cfg.str + cfg.pos ;
    size_t len = strlen(line) ;

    /**
     * The following while loop receives a string starting after the last
     * closed bracket found by the previous lexer process and checks if
     * the closed bracket is valid or not. For instance,
     * in an @execute field, we can also encounter a pair of opened
     * and closed brackets if it's written in, for example, shell script.
     */
    while (bracket && o < len) {

        char c = parse_char_next(line, len, &o) ;

        switch(c) {

            case '(':
                if (vp) {
                    vp-- ;
                    lvp = 0 ;
                }
                bracket++ ;
                break ;

            case ')':
                {
                    if (vp) {
                        vp = 0 ;
                        lvp = 0 ;
                    }

                    if (bracket - 1 == 0) {
                        /*
                         * This check the validity of the closed parenthese.
                         * For instance, a 'case...esac' statement in scripts
                         * use a ')' without its corresponding reverse part.
                         *
                         * When examining the very first character of the next
                         * line and encountering either '#@' or '@', it determines
                         * the validity. If the validity check fails, it signifies
                         * that we remain within the script context*/

                        int /* last parenthese */ lp = o ;
                        e = 0 ;

                        e = get_len_until(line + o, '\n') + 1 ;

                        if (o + e >= len) {
                            // end of string. this validate the parenthese
                            bracket-- ;
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
                         * validate the bracket. If neither of these is present,
                         * it signifies that we are inside a script.*/
                        while(line[o] == ' ' || line[o] == '\t' || line[o] == '\r')
                            o++ ;

                        if (line[o] != '#' && line[o] != '@')
                            break ;

                        if (line[o] == '#') {

                            if (line[o + 1] == '@') {
                                /** a commented key validates the parenthese */
                                o = lp ;
                                bracket-- ;
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
                        bracket-- ;
                        vp = 0 ;
                        break ;

                    } else
                       bracket-- ;

                    break ;
                }

            case '#':

                if (vp) {

                    /** we previously coming from a comment.
                     * this validates the parenthese.*/
                    if (line[o + 1] == '@') {
                        o = lvp ;
                        bracket-- ;
                        vp = 0 ;
                    } else {
                        /** another comment, continue the check at the
                         * next line */
                        e = get_len_until(line + o, '\n') + 1 ;
                        o += e ;
                    }
                }
                break ;
            case '[':

                if (vp) {
                    /** check the validity of the section name.
                     * we can found a '[]' pair in shell scripts,
                     * else we are inside a script.
                    */
                    char secname[20] ;
                    int r = get_sep_before(line + o, ']', '\n') ;
                    if (r < 0) {
                        e = get_len_until(line + o, '\n') + 1 ;
                        o += e ;
                        break ;
                    }

                    memcpy(secname, line + o, r) ;
                    secname[r] = 0 ;
                    unsigned int pos = 0 ;
                    while (enum_str_section[pos]) {
                        if (!strcmp(secname, enum_str_section[pos])) {
                            o = lvp ;
                            bracket-- ;
                            vp = 0 ;
                            break ;
                        }
                        pos++ ;
                    }
                }
                break ;
            case '@':
                /** we previously coming from a comment.
                 * this validates the parenthese.*/
                if (vp) {
                    o = lvp ;
                    bracket-- ;
                    vp = 0 ;
                }
                break ;
            case '\n':
                break ;
            case -1:
                return 0 ;
            default:
                if (vp) {
                    /** this invalidate the bracket*/
                    vp = 0 ;
                }
                break ;
        }
    }

    if (vp && (o == len)) {
        /** end of string. this validate the parenthese */
        bracket-- ;
        o = lvp ;
    }

    o -= 1 ; // remove the last bracket

    if (bracket)
        return 0 ;

    cfg.cpos = o + cfg.pos ;

    store->len = 0 ;

    if (!stack_add(store, cfg.str + cfg.opos + 1, cfg.cpos - cfg.opos - 1) ||
        !stack_close(store))
            return 0 ;

    return 1 ;
}