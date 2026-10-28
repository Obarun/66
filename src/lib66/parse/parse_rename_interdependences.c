/*
 * parse_rename_interdependences.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/hash.h>

#include <66/parse.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/event_rule.h>
#include <66/constants.h>

static int parse_prefix(strbuf *result, strbuf *stk, hash_t *hres, char const *prefix)
{
    log_flow() ;

    size_t pos = 0 ;
    struct resolve_hash_s *hash ;
    char store[SS_MAX_SERVICE_NAME + 1] ;

    FOREACH_SBL(stk, pos) {

        char const *name = stk->s + pos ;

        /** a member is always registered namespaced, so the name the selection
         * knows is already the right one: an external dependency keeps its bare
         * name, a fellow member gets the prefixed one, parse_get_hashname()
         * never composing a name longer than SS_MAX_SERVICE_NAME. A RequiredBy
         * is never parsed: a name it gives that is no member stays as written
         * (contract, parse: RequiredBy naming a service never parsed). */
        hash = parse_get_hashname(store, hres, name, prefix) ;

        if (!sbl_add(result, hash ? hash->res.sa.s + hash->res.name : name))
            return -1 ;
    }

    return 1 ;
}

static int parse_prefix_event(struct resolve_hash_s *c, hash_t *hres, char const *prefix)
{
    log_flow() ;

    size_t pos = 0 ;
    resolve_service_addon_event_t *ev = &c->event ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE_EVENT, ev) ;
    _cleanup_strbuf_ strbuf from = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf renamed = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf on = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf scoped = STRBUF_ZERO ;

    if (!sbl_clean_string(&from, ev->sa.s + ev->from))
        return -1 ;

    {
        FOREACH_SBL(&from, pos) {

            char known[SS_MAX_SERVICE_NAME + 1] ;
            struct resolve_hash_s *hash = parse_get_hashname(known, hres, from.s + pos, prefix) ;

            if (!sbl_add(&renamed, hash ? hash->res.sa.s + hash->res.name : from.s + pos))
                return -1 ;
        }
    }

    if (ev->type == EVENT_SOURCE_SERVICE && ev->non) {

        if (!sbl_clean_string(&on, ev->sa.s + ev->on))
            return -1 ;

        pos = 0 ;

        FOREACH_SBL(&on, pos) {

            char const *token = on.s + pos ;
            size_t slen = event_on_len(token) ;
            char source[SS_MAX_SERVICE_NAME + 1], known[SS_MAX_SERVICE_NAME + 1] ;
            struct resolve_hash_s *hash = 0 ;

            if (slen && slen <= SS_MAX_SERVICE_NAME) {

                memcpy(source, token, slen) ;
                source[slen] = 0 ;

                // a source part that is not a From member makes the token a bare condition
                if (sbl_search(&from, source) >= 0)
                    hash = parse_get_hashname(known, hres, source, prefix) ;
            }

            if (!hash) {

                if (!sbl_add(&scoped, token))
                    return -1 ;

                continue ;
            }

            _cleanup_strbuf_ strbuf one = STRBUF_ZERO ;

            if (!auto_strbuf(&one, hash->res.sa.s + hash->res.name, token + slen) || !sbl_add(&scoped, one.s))
                return -1 ;
        }

        if (resolve_add_list(&wres, &scoped, &ev->on) < 0)
            return -1 ;
    }

    if (resolve_add_list(&wres, &renamed, &ev->from) < 0)
        return -1 ;

    return 1 ;
}

static int parse_prefix_name(resolve_service_addon_dependencies_t *dep, hash_t *hres, char const *prefix)
{
    log_flow() ;

    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE_DEPENDENCIES, dep) ;
    uint32_t *field[2] = { &dep->depends, &dep->requiredby } ;
    uint32_t nfield[2] = { dep->ndepends, dep->nrequiredby } ;

    for (int i = 0 ; i < 2 ; i++) {

        _cleanup_strbuf_ strbuf stk = STRBUF_ZERO ;
        _cleanup_strbuf_ strbuf n = STRBUF_ZERO ;

        if (!nfield[i])
            continue ;

        if (!sbl_clean_string(&stk, dep->sa.s + *field[i]) || parse_prefix(&n, &stk, hres, prefix) < 0)
            return -1 ;

        if (resolve_add_list(&wres, &n, field[i]) < 0)
            return -1 ;
    }

    return 1 ;
}

int parse_rename_interdependences(resolve_service_addon_dependencies_t *dep, char const *prefix, hash_t *hres, ssexec_t *info)
{
    log_flow() ;

    struct resolve_hash_s *c, *tmp ;
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf members = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf stk = STRBUF_ZERO ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE_DEPENDENCIES, dep) ;

    HASH_FOREACH(hres, c, tmp) {

        if (c->res.inns && !strcmp(c->res.sa.s + c->res.inns, prefix) && !sbl_add(&members, c->name))
            return -1 ;
    }

    FOREACH_SBL(&members, pos) {

        c = resolve_hash_search(hres, members.s + pos) ;

        if ((c->dependencies.ndepends || c->dependencies.nrequiredby) && parse_prefix_name(&c->dependencies, hres, prefix) < 0)
            return -1 ;

        if (c->res.has_event && c->event.nfrom && parse_prefix_event(c, hres, prefix) < 0)
            return -1 ;

        if (c->res.logger && c->res.type == E_PARSER_TYPE_CLASSIC) {

            size_t namelen = strlen(c->res.sa.s + c->res.name) ;
            char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;

            auto_strings(logname, c->res.sa.s + c->res.name, SS_LOG_SUFFIX) ;

            if (parse_create_logger(hres, c, info) < 0)
                return -1 ;

            if (sbl_search(&stk, logname) < 0 && !sbl_add(&stk, logname))
                return -1 ;
        }

        if (sbl_search(&stk, members.s + pos) < 0 && !sbl_add(&stk, members.s + pos))
            return -1 ;
    }

    int off = parse_compute_list(&wres, &stk, &dep->ncontents, 0) ;
    if (off < 0)
        return -1 ;

    dep->contents = (uint32_t)off ;

    return 1 ;
}
