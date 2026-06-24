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
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum.h>
#include <66/enum_parser.h>

static char parse_char_next(char const *s, size_t slen, size_t *pos)
{
    char c = 0 ;
    if (*pos > slen) return -1 ;
    c = s[*pos] ;
    (*pos)++ ;
    return c ;
}

static int key_isvalid(const char *line, size_t *o, resolve_enum_table_t table)
{
    key_description_t const *list = table.u.parser.list  ;

    lexer_config cfg = LEXER_CONFIG_KEY ;
    _alloc_strbuf_(key, strlen(line + (*o) + 1)) ;
    cfg.str = line ;
    cfg.slen = strlen(line) ;

    if (!lexer(&key, &cfg) || !strbuf_uncounted(&key))
        return 0 ;

    if (cfg.found) {

        int r = key_to_enum(list, key.s) ;
        if (r < 0) {
            r = get_len_until(line + (*o), '\n') + 1 ;
            (*o) += r ;
            return 0 ;
        }
    }

    return 1 ;
}

int parse_bracket(strbuf *store, const char *str, resolve_enum_table_t table)
{
    log_flow() ;

    int /* need validation of the parentheses*/ vp = 1, /* last valid closed parenthese */ lvp = 0 ;
    uint8_t bracket = 1 ;
    size_t o = 0, e = 0 ;

    lexer_config cfg = LEXER_CONFIG_ZERO ;

    cfg.str = str ;
    cfg.slen = strlen(str) ;
    cfg.open = "(";
    cfg.olen = 1 ;
    cfg.close = ")\n" ;
    cfg.clen = 2 ;
    cfg.skip = " \t\r" ;
    cfg.skiplen = 3 ;
    cfg.kopen = 0 ;
    cfg.kclose = 0 ;
    cfg.style = 0 ;

    if (!lexer(store, &cfg) || !strbuf_uncounted(store))
        return 0 ;

    if (!cfg.found)
        return 0 ;

    char const *line =  cfg.str + cfg.pos ;
    size_t len = strlen(line) ;
    int olvp = cfg.cpos ;

    /**
     * The following while loop receives a string starting after the last
     * closed bracket found by the previous lexer process and checks if
     * the closed bracket is valid or not. For instance,
     * in an Execute field, we can also encounter a pair of opened
     * and closed brackets if it's written in, for example, shell script.
     */

    while (bracket && o < len) {

        char c = parse_char_next(line, len, &o) ;

        switch(c) {

            case '(':
                if (vp) {
                    vp = 0 ;
                    lvp = olvp ;
                }
                bracket++ ;
                break ;

            case ')':
                {

                    if (vp) {
                        vp = 0 ;
                        lvp = o + olvp ;
                        break ;
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

                        int /* last parenthese */ lp = o + olvp ;
                        e = 0 ;

                        e = get_len_until(line + o, '\n') + 1 ;

                        if (o + e >= len) {
                            // end of string. this validate the parenthese
                            bracket-- ;
                            o = len ;
                            lvp = lp ;
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

                        /** Outside of the context specified (e.g., Execute=()),
                         * only '#' and upper case character combinaison are considered valid to
                         * validate the bracket. If neither of these is present,
                         * it signifies that we are inside a script.*/
                        while(line[o] == ' ' || line[o] == '\t' || line[o] == '\r')
                            o++ ;

                        if (line[o] != '#' && (line[o] < 65 || line[o] > 90)) {
                            lvp = lp ;
                            o++ ;
                            vp = 0 ;
                            break ;
                        }

                        if (line[o] == '#') {

                            if (line[o + 1] >= 65 && line[o + 1] <= 90) {
                                /** a commented key validates the parenthese */
                                if (!key_isvalid(line, &o, table)) {
                                    lvp = lp ;
                                    vp = 0 ;
                                    break ;
                                }
                                lvp = lp ;
                                bracket = 0 ;
                                vp = 0 ;

                            } else {
                                // this is a comment
                                e = get_len_until(line + o, '\n') + 1 ;
                                o += e ;
                                vp = 1 ;
                                lvp = lp  ;
                            }
                            break ;
                        }

                        if (key_isvalid(line, &o, table))
                            bracket-- ;

                        lvp = lp ;
                        vp = 0 ;
                        break ;

                    } else
                       bracket-- ;

                    break ;
                }

            case '#':

                if (vp) {

                    /** we previously coming from a comment or empty line.
                     * this validates the parenthese.*/
                    if (line[o + 1] >= 65 && line[o + 1] <= 90) {
                        if (!key_isvalid(line, &o, table))
                            break ;

                        bracket = 0 ;
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
                        vp = 0 ;
                        break ;
                    }

                    /** too long to be a known section name (the longest is
                     * "Environment"): not a section, stay in script context. */
                    if (r >= (int)sizeof(secname))
                        break ;

                    memcpy(secname, line + o, r) ;
                    secname[r] = 0 ;
                    unsigned int pos = 0 ;
                    while (enum_str_parser_section[pos]) {
                        if (!strcmp(secname, enum_str_parser_section[pos])) {
                            bracket = 0 ;
                            vp = 0 ;
                            break ;
                        }
                        pos++ ;
                    }
                }
                break ;
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'G':
            case 'H':
            case 'I':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'P':
            case 'Q':
            case 'R':
            case 'S':
            case 'T':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
                /** we previously coming from a comment.
                 * this validates the parenthese or the beginning
                 * of the parse process.*/
                if (vp) {
                    if (!key_isvalid(line, &o, table)) {
                        vp = 0 ;
                        if (!lvp)
                            lvp = olvp ;
                        break ;
                    }

                    if (cfg.str[cfg.cpos] == ')') {
                        bracket-- ;
                        vp = 0 ;
                    }
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

    if (o == len) {
        /** end of string. this validate the parenthese */
        if (vp || cfg.str[lvp] == ')')
            bracket = 0 ;
    }

    if (bracket)
        return 0 ;

    if (!lvp)
        lvp = cfg.cpos ;

    store->len = 0 ;

    if (!sbl_addb(store, cfg.str + cfg.opos + 1, lvp - (cfg.opos + 1)))
        return 0 ;

    return 1 ;
}