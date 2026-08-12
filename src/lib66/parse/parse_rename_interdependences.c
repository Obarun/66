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
 * except according to the terms contained in the LICENSE file./
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

static void parse_prefix(char *result, strbuf *stk, hash_t *hres, char const *prefix)
{
    log_flow() ;

    size_t pos = 0 ;
    struct resolve_hash_s *hash ;
    char store[SS_MAX_SERVICE_NAME + 1] ;

    FOREACH_SBL(stk, pos) {

        hash = parse_get_hashname(store, hres, stk->s + pos, prefix) ;
        if (hash == NULL)
            log_die(LOG_EXIT_USER, "service: ", stk->s + pos, " not available -- please make a bug report") ;

        /** a member is always registered namespaced, so the name the selection
         * knows is already the right one: an external dependency keeps its bare
         * name, a fellow member gets the prefixed one. */
        auto_strings(result + strlen(result), hash->res.sa.s + hash->res.name, " ") ;
    }

    result[strlen(result) - 1] = 0 ;
}

static void parse_prefix_event(struct resolve_hash_s *c, hash_t *hres, char const *prefix)
{
    log_flow() ;

    size_t pos = 0, mlen = strlen(prefix) ;
    resolve_service_addon_event_t *ev = &c->event ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EVENT, ev) ;
    _alloc_sbl_(from, strlen(ev->sa.s + ev->from) + 1) ;
    _alloc_sbl_(renamed, (mlen + 1) * ev->nfrom + strlen(ev->sa.s + ev->from) + 1) ;

    if (!sbl_clean_string(&from, ev->sa.s + ev->from))
        log_dieusys(LOG_EXIT_SYS, "convert string to sbl") ;

    FOREACH_SBL(&from, pos) {

        char known[SS_MAX_SERVICE_NAME + 1] ;
        struct resolve_hash_s *hash = parse_get_hashname(known, hres, from.s + pos, prefix) ;

        if (!sbl_add(&renamed, hash ? hash->res.sa.s + hash->res.name : from.s + pos))
            log_die_nomem("strbuf") ;
    }

    if (ev->type == EVENT_SOURCE_SERVICE && ev->non) {

        _alloc_sbl_(on, strlen(ev->sa.s + ev->on) + 1) ;
        _alloc_sbl_(scoped, (mlen + 1) * ev->non + strlen(ev->sa.s + ev->on) + 1) ;

        if (!sbl_clean_string(&on, ev->sa.s + ev->on))
            log_dieusys(LOG_EXIT_SYS, "convert string to sbl") ;

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
                    log_die_nomem("strbuf") ;

                continue ;
            }

            char const *name = hash->res.sa.s + hash->res.name ;
            char n[strlen(name) + strlen(token + slen) + 1] ;

            auto_strings(n, name, token + slen) ; // token + slen points at the ':'

            if (!sbl_add(&scoped, n))
                log_die_nomem("strbuf") ;
        }

        if (!sbl_rebuild_with_delim(&scoped, ' '))
            log_dieusys(LOG_EXIT_SYS, "rebuild stack list") ;

        char n[scoped.len + 1] ;
        auto_strings(n, scoped.s) ;

        ev->on = resolve_add_string(wres, n) ;
    }

    if (!sbl_rebuild_with_delim(&renamed, ' '))
        log_dieusys(LOG_EXIT_SYS, "rebuild stack list") ;

    char n[renamed.len + 1] ;
    auto_strings(n, renamed.s) ;

    ev->from = resolve_add_string(wres, n) ;

    free(wres) ;
}

static void parse_prefix_name(resolve_service_addon_dependencies_t *dep, hash_t *hres, char const *prefix)
{
    log_flow() ;

    size_t mlen = strlen(prefix) ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;

    if (dep->ndepends) {

        size_t depslen = strlen(dep->sa.s + dep->depends) ;
        _alloc_sbl_(stk, depslen + 1) ;

        if (!sbl_clean_string(&stk, dep->sa.s + dep->depends))
            log_dieusys(LOG_EXIT_SYS, "convert string to sbl") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * dep->ndepends ;
        char n[len] ;

        memset(n, 0, len * sizeof(char)); ;

        parse_prefix(n, &stk, hres, prefix) ;

        dep->depends = resolve_add_string(wres, n) ;

    }

    if (dep->nrequiredby) {

        size_t depslen = strlen(dep->sa.s + dep->requiredby) ;
        _alloc_sbl_(stk, depslen + 1) ;

        if (!sbl_clean_string(&stk, dep->sa.s + dep->requiredby))
            log_dieusys(LOG_EXIT_SYS, "convert string to strbuf") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * dep->nrequiredby ;
        char n[len] ;

        memset(n, 0, len * sizeof(char)) ;

        parse_prefix(n, &stk, hres, prefix) ;

        dep->requiredby = resolve_add_string(wres, n) ;

    }

    free(wres) ;
}

void parse_rename_interdependences(resolve_service_t *res, resolve_service_addon_dependencies_t *dep, char const *prefix, hash_t *hres, ssexec_t *info)
{
    log_flow() ;

    struct resolve_hash_s *c, *tmp ;
    _alloc_sbl_(stk, resolve_hash_count(hres) * SS_MAX_SERVICE_NAME + 1) ;
    resolve_wrapper_t_ref wres = 0 ;

    HASH_FOREACH(hres, c, tmp) {

        if (!strcmp(c->res.sa.s + c->res.inns, prefix)) {

            if (c->dependencies.ndepends || c->dependencies.nrequiredby)
                parse_prefix_name(&c->dependencies, hres, prefix) ;

            if (c->res.has_event && c->event.nfrom)
                parse_prefix_event(c, hres, prefix) ;

            if (c->res.logger && (c->res.type == E_PARSER_TYPE_CLASSIC || c->res.type == E_PARSER_TYPE_ONESHOT)) {

                size_t namelen = strlen(c->res.sa.s + c->res.name) ;
                char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;

                // the logger name is always <service>-log; the logger config lives
                // in c->logger, untouched by the rename above.
                auto_strings(logname, c->res.sa.s + c->res.name, SS_LOG_SUFFIX) ;

                parse_create_logger(hres, c, info) ;

                if (c->res.type == E_PARSER_TYPE_CLASSIC) {
                    if (!sbl_add(&stk, logname))
                        log_die_nomem("strbuf") ;
                }

            }

            if (sbl_search(&stk, c->res.sa.s + c->res.name) < 0 )
                if (!sbl_add(&stk, c->res.sa.s + c->res.name))
                    log_die_nomem("strbuf") ;
        }
    }

    (void)res ;
    wres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;

    dep->contents = parse_compute_list(wres, &stk, &dep->ncontents, 0) ;

    free(wres) ;
}
