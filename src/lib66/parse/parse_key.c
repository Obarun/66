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
 * except according to the terms contained in the LICENSE file.
 */

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum.h>

lexer_config LEXER_CONFIG_KEY = { \
    .str = 0,\
    .slen = 0,\
    .open = "ABCDEFGHIJKLMNOPQRSTUVWXYZ",\
    .olen = 26,\
    .close = "=\n",\
    .clen = 2,\
    .skip = " \t\r",\
    .skiplen = 3,\
    .kopen = 1,\
    .kclose = 0,\
    .style = 0,\
    .forceopen = 0,\
    .forceclose = 0,\
    .pos = 0,\
    .opos = 0,\
    .cpos = 0,\
    .invalue = 0,\
    .found = 0,\
    .exitcode = 0,\
} ;

static size_t key_linestart(char const *str, size_t opos)
{
    size_t o = 0, n ;

    while ((n = o + str_search(str + o, '\n')) < opos)
        o = n + 1 ;

    while (str[o] == ' ' || str[o] == '\t' || str[o] == '\r')
        o++ ;

    return o ;
}

int parse_key(strbuf *key, lexer_config *cfg, resolve_enum_table_t table)
{
    log_flow() ;

    int kid = -1, next = -1 ;

    if (!lexer(key, cfg) || !strbuf_terminate(key))
        return -1 ;
    key->len-- ;

    if (cfg->found) {

        size_t c, o = key_linestart(cfg->str, cfg->opos) ;

        if (parse_section_header(cfg->str, o, &c)) {
            cfg->found = 0 ;
            cfg->pos = cfg->slen ;
            return 0 ;
        }

        if (cfg->str[o] != '#') {
            kid = key_to_enum(table.u.parser.list, key->s) ;
            if (kid >= 0)
                return kid ;
            log_warn("unknown key: ", key->s, " -- ignoring it") ;
        }

        cfg->found = 0 ;
        next = str_search(cfg->str + cfg->opos, '\n') ;
        if (!cfg->str[cfg->opos + next]) next = 0 ;
        cfg->pos = cfg->opos + next + 1 ;
    }

    return 0 ;
}