/*
 * parse_key.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum.h>

int parse_key(strbuf *key, lexer_config *cfg, resolve_enum_table_t table)
{
    log_flow() ;

    int kid = -1, next = -1 ;

    if (!lexer(key, cfg) || !strbuf_terminate(key))
        return -1 ;
    key->len-- ;

    if (cfg->found) {
        kid = key_to_enum(table.u.parser.list, key->s) ;
        if (kid < 0) {
            log_warn("unknown key: ", key->s, " -- ignoring it") ;
            cfg->found = 0 ;
            next = get_len_until(cfg->str + cfg->opos, '\n') ;
            if (next < 0) next = 0 ;
            cfg->pos = cfg->opos + next + 1 ;
        }

        /** check for commented key (no char precedes a key at offset 0) */
        if (cfg->opos > 0 && cfg->str[cfg->opos - 1] == '#')
            cfg->found = 0 ;
    }

    return kid < 0 ? 0 : kid ;
}