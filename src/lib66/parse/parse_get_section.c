/*
 * parse_get_section.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum.h>

int parse_get_section(lexer_config *acfg, unsigned int *ncfg, char const *str, size_t len)
{
    log_flow() ;

    size_t pos = 0 ;
    _alloc_stk_(stk, len + 1) ;

    while (pos < len) {

        lexer_config cfg = LEXER_CONFIG_SECTION ;
        cfg.str = str ;
        cfg.slen = len ;
        cfg.pos = pos ;
        stack_reset(&stk) ;

        if (!lexer(&stk, &cfg) || !stack_close(&stk))
            return 0 ;

        if (cfg.found) {

            ssize_t id = get_enum_by_key(list_section, stk.s) ;
            if (id < 0 || cfg.str[!cfg.opos ? 0 : cfg.opos - 1] == '#') {
                log_warn(id < 0 ? "invalid section name: " : "commented section: ", stk.s, " -- ignoring it") ;
                id = get_len_until(cfg.str + cfg.cpos, '\n') ;
                if (id < 0)
                    log_warn_return(LOG_EXIT_ZERO, "frontend service file must end with an empty line") ;

                cfg.pos += --id ;
                pos = cfg.pos ;
                continue ;
            }

            if (!pos && strcmp(stk.s, enum_str_section[SECTION_MAIN]))
                log_warn_return(LOG_EXIT_ZERO, "invalid frontend file -- section [Main] must be set first") ;

            log_trace("found section: ", stk.s) ;

            acfg[(*ncfg)++] = cfg ;
        }
        pos = cfg.pos ;
    }

    if (!(*ncfg))
        log_warn_return(LOG_EXIT_ZERO, "invalid frontend file -- no section found") ;

    return 1 ;
}