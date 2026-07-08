/*
 * parse_store.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/enum_parser.h>

// the (sid,kid) arrays must hold the widest section's key set (safety fail at compilation)
typedef char parse_store_nkey_is_large_enough[
    (E_PARSER_SECTION_MAIN_ENDOFKEY <= SS_PARSE_STORE_NKEY &&
     E_PARSER_SECTION_STARTSTOP_ENDOFKEY <= SS_PARSE_STORE_NKEY &&
     E_PARSER_SECTION_LOGGER_ENDOFKEY <= SS_PARSE_STORE_NKEY &&
     E_PARSER_SECTION_ENVIRON_ENDOFKEY <= SS_PARSE_STORE_NKEY &&
     E_PARSER_SECTION_REGEX_ENDOFKEY <= SS_PARSE_STORE_NKEY &&
     E_PARSER_SECTION_EXECUTE_ENDOFKEY <= SS_PARSE_STORE_NKEY &&
     E_PARSER_SECTION_EVENT_ENDOFKEY <= SS_PARSE_STORE_NKEY) ? 1 : -1] ;

static resolve_enum_table_t store_table(uint32_t sid)
{
    switch (sid) {

        case E_PARSER_SECTION_START: {
            resolve_enum_table_t t = E_TABLE_PARSER_SECTION_START_ZERO ;
            return t ;
        }
        case E_PARSER_SECTION_STOP: {
            resolve_enum_table_t t = E_TABLE_PARSER_SECTION_STOP_ZERO ;
            return t ;
        }
        case E_PARSER_SECTION_LOGGER: {
            resolve_enum_table_t t = E_TABLE_PARSER_SECTION_LOGGER_ZERO ;
            return t ;
        }
        case E_PARSER_SECTION_REGEX: {
            resolve_enum_table_t t = E_TABLE_PARSER_SECTION_REGEX_ZERO ;
            return t ;
        }
        case E_PARSER_SECTION_EXECUTE: {
            resolve_enum_table_t t = E_TABLE_PARSER_SECTION_EXECUTE_ZERO ;
            return t ;
        }
        case E_PARSER_SECTION_EVENT: {
            resolve_enum_table_t t = E_TABLE_PARSER_SECTION_EVENT_ZERO ;
            return t ;
        }
        case E_PARSER_SECTION_MAIN:
        default: {
            resolve_enum_table_t t = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
            return t ;
        }
    }
}

static int store_put(parse_store_t *st, uint32_t sid, uint32_t kid, char const *s, size_t slen)
{
    char nul = 0 ;
    uint32_t off = (uint32_t)st->arena.len ;

    if (!strbuf_catb(&st->arena, s, slen) || !strbuf_catb(&st->arena, &nul, 1))
        return 0 ;

    st->off[sid][kid] = off ;
    st->len[sid][kid] = (uint32_t)slen ;
    st->present[sid][kid] = 1 ;

    return 1 ;
}

static int store_environ(parse_store_t *st, char const *body)
{
    int r = get_len_until(body, '\n') ;
    if (r < 0)
        r = 0 ;
    r++ ;

    char const *val = body + r ;

    return store_put(st, E_PARSER_SECTION_ENVIRONMENT, E_PARSER_SECTION_ENVIRON_ENVAL, val, strlen(val)) ;
}

// !!Last writer wins.
static int store_keys(parse_store_t *st, uint32_t sid, char const *body, resolve_enum_table_t table)
{
    lexer_config kcfg = LEXER_CONFIG_KEY ;
    int kid = -1 ;

    kcfg.str = body ;
    kcfg.slen = strlen(body) ;

    _alloc_sbl_(key, kcfg.slen + 1) ;

    while (kcfg.pos < kcfg.slen) {

        kcfg.found = 0 ;
        key.len = 0 ;
        lexer_reset(&kcfg) ;
        kcfg.opos = kcfg.cpos = 0 ;

        kid = parse_key(&key, &kcfg, table) ;
        if (kid < 0)
            log_warnu_return(LOG_EXIT_ZERO, "get keys in section: ", enum_str_parser_section[sid]) ;

        table.u.parser.id = kid ;

        if (kcfg.found) {

            _alloc_sbl_(val, kcfg.slen + 1) ;

            if (!parse_value(&val, &kcfg, table))
                log_warnu_return(LOG_EXIT_ZERO, "get value of key: ", key.s) ;

            if (!store_put(st, sid, (uint32_t)kid, val.s, val.len))
                log_warnu_return(LOG_EXIT_ZERO, "store value of key: ", key.s) ;
        }
    }

    return 1 ;
}

int parse_store_build(parse_store_t *st, char const *frontend)
{
    log_flow() ;

    parse_validator_t v ;

    memset(st, 0, sizeof(*st)) ;
    st->frontend = frontend ;

    if (!parse_validator_init(&v, frontend))
        return 0 ;

    for (uint32_t sid = 0 ; sid < E_PARSER_SECTION_ENDOFKEY ; sid++) {

        if (!v.present[sid])
            continue ;

        size_t l = v.len[sid] ;
        char body[l + 2] ;
        memcpy(body, v.frontend + v.off[sid], l) ;
        body[l] = '\n' ; // key lexing looks for '=\n'
        body[l + 1] = 0 ;

        if (sid == E_PARSER_SECTION_ENVIRONMENT) {
            if (!store_environ(st, body))
                return 0 ;
            continue ;
        }

        if (!store_keys(st, sid, body, store_table(sid)))
            return 0 ;
    }

    return 1 ;
}

char const *parse_store_get(parse_store_t *st, uint32_t sid, uint32_t kid, size_t *len)
{
    if (sid >= E_PARSER_SECTION_ENDOFKEY || kid >= SS_PARSE_STORE_NKEY || !st->present[sid][kid])
        return 0 ;

    if (len)
        *len = st->len[sid][kid] ;

    return st->arena.s + st->off[sid][kid] ;
}

uint8_t parse_store_present(parse_store_t *st, uint32_t sid, uint32_t kid)
{
    if (sid >= E_PARSER_SECTION_ENDOFKEY || kid >= SS_PARSE_STORE_NKEY)
        return 0 ;

    return st->present[sid][kid] ;
}

void parse_store_free(parse_store_t *st)
{
    strbuf_free(&st->arena) ;
}
