/*
 * parse_get_value_of_key.c
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

#include <oblibs/log.h>
#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum.h>

int parse_get_value_of_key(stack *store, char const *str, char const *key)
{
    lexer_config cfg = LEXER_CONFIG_KEY ;
    key_all_t const *list = total_list ;
    int kid = -1, sid = -1, spos = 0, kpos = 0 ;

    cfg.str = str ;
    cfg.slen = strlen(str) ;
    _alloc_stk_(k, cfg.slen + 1) ;

    while (cfg.pos < cfg.slen) {

        k.len = 0 ;
        cfg.found = 0 ;
        kid = parse_key(&k, &cfg) ;
        if (kid < 0)
            log_warnu_return(LOG_EXIT_ZERO, "parse key: ", key) ;

        if (cfg.found && !strcmp(k.s, key)) {

            spos = 0 ;
            sid = -1 ;
            /* search for the section id of the key */
            while (list[spos].list) {

                kpos = 0 ;
                while(*list[spos].list[kpos].name) {

                    if (!strcmp(k.s, *list[spos].list[kpos].name)) {
                        sid = spos ;
                        break ;
                    }
                    kpos++ ;
                }
                if (sid == spos)
                    break ;
                spos++ ;
            }

            if (sid < 0)
                log_warnu_return(LOG_EXIT_ZERO, "get section id of key: ", k.s, " -- please make a bug report") ;

            if (!parse_value(store, &cfg, sid, kid))
                log_warnu_return(LOG_EXIT_ZERO, "get value of key: ", k.s) ;

            break ;
        }
    }

    return cfg.found ;
}