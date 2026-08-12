/*
 * parse_event.c
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

#include <stdint.h>
#include <stdlib.h> // free
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h> // u64_scan, u64_scan_strict, sig_parse
#include <oblibs/sse.h> // parse_cron, cron_t

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/config.h> // SS_MAX_SERVICE_NAME
#include <66/enum_parser.h>
#include <66/event_rule.h>
#include <66/status.h> // status_state_from_string, status_result_from_string

#define EVENT_PRESENT(st, k) parse_store_present((st), E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_##k)
#define MAIN_PRESENT(st, k) parse_store_present((st), E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_##k)
#define MAIN_GET(st, k) parse_store_get((st), E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_##k, 0)

static int read_list(parse_store_t *st, uint32_t sid, uint32_t kid, resolve_wrapper_t_ref wres, uint32_t *off, uint32_t *n)
{
    log_flow() ;

    size_t len = 0 ;
    char const *v = parse_store_get(st, sid, kid, &len) ;

    _alloc_sbl_(stk, len + 1) ;
    if (!strbuf_copyb(&stk, v, len))
        log_die_nomem("strbuf") ;

    if (!parse_list(&stk))
        return 0 ;

    if (stk.len)
        *off = parse_compute_list(wres, &stk, n, 0) ;

    return 1 ;
}

static int every_to_ms(char const *v, uint32_t *ms)
{
    uint64_t n = 0 ;
    size_t p = u64_scan(v, &n) ;
    if (!p)
        return 0 ;

    uint64_t mult ;
    switch (v[p]) {
        case 0:
        case 's': mult = 1000 ; break ;
        case 'm': mult = 60000 ; break ;
        case 'h': mult = 3600000 ; break ;
        case 'd': mult = 86400000 ; break ;
        default: return 0 ;
    }

    if (v[p] && v[p + 1])
        return 0 ; // trailing garbage after the suffix

    uint64_t r = n * mult ;
    if (r < 1000)
        r = 1000 ;
    if (r > UINT32_MAX)
        return 0 ;

    *ms = (uint32_t)r ;

    return 1 ;
}

static int token_in_list(char const *list, char const *tok, size_t tlen)
{
    char const *p = list ;

    while (*p) {

        char const *sp = p ;
        while (*sp && *sp != ' ') sp++ ;

        if ((size_t)(sp - p) == tlen && !strncmp(p, tok, tlen))
            return 1 ;

        p = sp ;
        while (*p == ' ') p++ ;
    }

    return 0 ;
}

static int on_service_predicate_ok(char const *cond)
{
    char const *colon = strchr(cond, ':') ;
    if (!colon)
        return status_state_from_string(cond) >= 0 || status_result_from_string(cond) >= 0 ;

    size_t llen = (size_t)(colon - cond) ;
    char word[llen + 1] ;
    memcpy(word, cond, llen) ;
    word[llen] = 0 ;

    switch (status_result_from_string(word)) {

        case STATUS_RESULT_EXITED: {
            uint64_t n ;
            return colon[1] && u64_scan_strict(colon + 1, &n) ;
        }
        case STATUS_RESULT_SIGNALED: {
            int sig ;
            return colon[1] && sig_parse(colon + 1, &sig) ;
        }
        default:
            return 0 ; // any other word does not take an argument
    }
}

static int on_service_token_ok(char const *token, char const *from, int has_from)
{
    size_t slen = event_on_len(token) ;

    if (!slen)
        return on_service_predicate_ok(token) ;

    // <source>:<condition> -- the source must be an explicit From member
    if (!has_from || !token_in_list(from, token, slen))
        return 0 ;

    return on_service_predicate_ok(token + slen + 1) ;
}

static int validate_on_tokens(char const *on, int src, char const *from, int has_from, char const *name)
{
    char const *p = on ;

    while (*p) {

        char const *sp = p ;
        while (*sp && *sp != ' ') sp++ ;

        size_t len = (size_t)(sp - p) ;
        char token[len + 1] ;
        memcpy(token, p, len) ;
        token[len] = 0 ;

        int ok = 1 ;
        if (src == EVENT_SOURCE_SERVICE)
            ok = on_service_token_ok(token, from, has_from) ;
        else if (src == EVENT_SOURCE_SIGNAL) {
            int sig ;
            ok = sig_parse(token, &sig) != 0 ;
        } else if (src == EVENT_SOURCE_INOTIFY)
            ok = event_in_is_valid(token) ;

        if (!ok)
            log_warnu_return(LOG_EXIT_ZERO, "invalid On token: ", token, " of service: ", name) ;

        p = sp ;
        while (*p == ' ') p++ ;
    }

    return 1 ;
}

/** A tick reactor states an EventType that From already implies, and 66-eventd
 * delivers an event only to a reactor whose type equals the source's own type.
 * A mismatch parses, arms and then never fires -- silently. Refuse it here.
 * Only the tick families reach this: their From is folded into depends, which is
 * what makes the source resolvable at this point. */
static int validate_from_sources(char const *from, int src, parse_build_ctx_t *ctx, char const *name)
{
    _alloc_sbl_(stk, strlen(from) + 1) ;

    if (!sbl_clean_string(&stk, from))
        log_warnusys_return(LOG_EXIT_ZERO, "clean the From list of service: ", name) ;

    size_t pos = 0 ;

    FOREACH_SBL(&stk, pos) {

        int type ;
        char known[SS_MAX_SERVICE_NAME + 1] ;
        struct resolve_hash_s *h = parse_get_hashname(known, ctx->hres, stk.s + pos, ctx->inns) ;

        if (h) {

            if (h->res.type != E_PARSER_TYPE_EVENT)
                log_warn_return(LOG_EXIT_ZERO, "From: ", known, " is not an event source of service: ", name) ;

            type = (int)h->event.type ;

        } else {

            // the source was parsed by an earlier run: read it back from disk
            resolve_service_t res = RESOLVE_SERVICE_ZERO ;
            resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE, &res) ;
            int r = resolve_read(w, ctx->info->base.s, known) ;
            uint32_t rtype = res.type ;
            resolve_free(w) ;

            if (r <= 0)
                log_warnu_return(LOG_EXIT_ZERO, "read the resolve file of source: ", known, " of service: ", name) ;

            // a reactor carries an event addon too, so test the service type first
            if (rtype != E_PARSER_TYPE_EVENT)
                log_warn_return(LOG_EXIT_ZERO, "From: ", known, " is not an event source of service: ", name) ;

            resolve_service_addon_event_t ev = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
            w = resolve_set_struct(DATA_SERVICE_EVENT, &ev) ;
            r = resolve_read(w, ctx->info->base.s, known) ;
            uint32_t etype = ev.type ;
            resolve_free(w) ;

            if (r <= 0)
                log_warnu_return(LOG_EXIT_ZERO, "read the event addon of source: ", known, " of service: ", name) ;

            type = (int)etype ;
        }

        if (type != src)
            log_warn_return(LOG_EXIT_ZERO, "EventType: ", event_src_to_string(src),
                            " does not match its source: ", known, " which is a ",
                            event_src_to_string(type), " source, of service: ", name) ;
    }

    return 1 ;
}

int parse_event(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    parse_store_t *st = ctx->st ;
    resolve_service_t *res = &c->res ;
    resolve_service_addon_event_t *ev = &c->event ;
    char const *name = res->sa.s + res->name ;

    res->has_event = 0 ;

    int any = 0 ;
    for (uint32_t kid = 0 ; kid < E_PARSER_SECTION_EVENT_ENDOFKEY ; kid++) {

        if (parse_store_present(st, E_PARSER_SECTION_EVENT, kid)) {
            any = 1 ;
            break ;
        }
    }

    if (!any)
        return 1 ;

    if (!EVENT_PRESENT(st, EVENTTYPE))
        log_warn_return(LOG_EXIT_ZERO, "[Event] section requires the EventType key of service: ", name) ;

    char const *v = parse_store_get(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_EVENTTYPE, 0) ;
    int src = event_src_from_string(v) ;
    if (src < 0)
        log_warn_return(LOG_EXIT_ZERO, "invalid EventType: ", v, " of service: ", name) ;

    int has_from = EVENT_PRESENT(st, FROM) ;
    int has_on = EVENT_PRESENT(st, ON) ;
    int has_onall = EVENT_PRESENT(st, ONALL) ;
    int has_do = EVENT_PRESENT(st, DO) ;
    int has_emit = EVENT_PRESENT(st, EMIT) ;

    if (src == EVENT_SOURCE_USER) {

        if (has_from)
            log_warn_return(LOG_EXIT_ZERO, "a user reactor is sourceless: From not allowed of service: ", name) ;

    } else if (!has_from)
        log_warn_return(LOG_EXIT_ZERO, "an event reactor requires From of service: ", name) ;

    /* condition: On / OnAll */
    switch (src) {

        case EVENT_SOURCE_SERVICE:
        case EVENT_SOURCE_SIGNAL:

            if (has_on && has_onall)
                log_warn_return(LOG_EXIT_ZERO, "On and OnAll are mutually exclusive of service: ", name) ;

            if (!has_on && !has_onall)
                log_warn_return(LOG_EXIT_ZERO, "a service/signal reactor requires On or OnAll of service: ", name) ;
            break ;

        case EVENT_SOURCE_USER:

            if (has_onall)
                log_warn_return(LOG_EXIT_ZERO, "OnAll is not allowed for a user reactor of service: ", name) ;

            if (!has_on)
                log_warn_return(LOG_EXIT_ZERO, "a user reactor requires On of service: ", name) ;
            break ;

        case EVENT_SOURCE_INOTIFY:
        case EVENT_SOURCE_SCHEDULE:
        case EVENT_SOURCE_TIMER:

            if (has_on || has_onall)
                log_warn_return(LOG_EXIT_ZERO, "On/OnAll is not allowed for an inotify/schedule/timer reactor of service: ", name) ;
            break ;

        default:
            log_warn_return(LOG_EXIT_ZERO, "unknown event source: ", event_src_to_string(src)) ;
    }


    if (!has_do && !has_emit)
        log_warn_return(LOG_EXIT_ZERO, "an event reactor requires Do or Emit of service: ", name) ;

    ev->type = (uint32_t)src ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EVENT, ev) ;
    resolve_init(wres) ;

    if (has_from && !read_list(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_FROM, wres, &ev->from, &ev->nfrom)) {
        free(wres) ;
        log_warnu_return(LOG_EXIT_ZERO, "read the From list of service: ", name) ;
    }

    if (has_on) {

        ev->combine = EVENT_COMBINE_ANY ;

        if (!read_list(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_ON, wres, &ev->on, &ev->non)) {
            free(wres) ;
            log_warnu_return(LOG_EXIT_ZERO, "read the On list of service: ", name) ;
        }

    } else if (has_onall) {

        ev->combine = EVENT_COMBINE_ALL ;

        if (!read_list(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_ONALL, wres, &ev->on, &ev->non)) {
            free(wres) ;
            log_warnu_return(LOG_EXIT_ZERO, "read the OnAll list of service: ", name) ;
        }
    }

    if ((src == EVENT_SOURCE_SERVICE || src == EVENT_SOURCE_SIGNAL) &&
        !validate_on_tokens(ev->sa.s + ev->on, src, ev->sa.s + ev->from, has_from, name)) {
        free(wres) ;
        return 0 ;
    }

    if ((src == EVENT_SOURCE_INOTIFY || src == EVENT_SOURCE_SCHEDULE || src == EVENT_SOURCE_TIMER) &&
        !validate_from_sources(ev->sa.s + ev->from, src, ctx, name)) {
        free(wres) ;
        return 0 ;
    }

    if (has_do) {

        v = parse_store_get(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_DO, 0) ;
        int d = event_do_from_string(v) ;

        if (d < 0) {
            free(wres) ;
            log_warn_return(LOG_EXIT_ZERO, "invalid Do action: ", v, " of service: ", name) ;
        }

        ev->docmd = (uint32_t)d ;

        if (d == EVENT_DO_RELOAD && res->type == E_PARSER_TYPE_ONESHOT) {
            free(wres) ;
            log_warn_return(LOG_EXIT_ZERO, "Do=reload needs a process to signal, not allowed on a oneshot reactor of service: ", name) ;
        }
    }

    if (has_emit) {
        v = parse_store_get(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_EMIT, 0) ;
        ev->emit = resolve_add_string(wres, v) ;
    }

    free(wres) ;

    res->has_event = 1 ;

    return 1 ;
}

int parse_event_source(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    parse_store_t *st = ctx->st ;
    resolve_service_t *res = &c->res ;
    resolve_service_addon_event_t *ev = &c->event ;
    char const *name = res->sa.s + res->name ;

    res->has_event = 0 ;

    if (!MAIN_PRESENT(st, EVENTTYPE))
        log_warn_return(LOG_EXIT_ZERO, "a Type=event source requires the EventType key of service: ", name) ;

    char const *v = MAIN_GET(st, EVENTTYPE) ;
    int src = event_src_from_string(v) ;
    if (src < 0)
        log_warn_return(LOG_EXIT_ZERO, "invalid EventType: ", v, " of service: ", name) ;

    if (src != EVENT_SOURCE_INOTIFY && src != EVENT_SOURCE_SCHEDULE && src != EVENT_SOURCE_TIMER)
        log_warn_return(LOG_EXIT_ZERO, "a source EventType must be inotify, schedule or timer of service: ", name) ;

    int has_watch = MAIN_PRESENT(st, WATCH) ;
    int has_on = MAIN_PRESENT(st, ON) ;
    int has_expr = MAIN_PRESENT(st, EXPRESSION) ;
    int has_tz = MAIN_PRESENT(st, TIMEZONE) ;
    int has_every = MAIN_PRESENT(st, EVERY) ;

    ev->type = (uint32_t)src ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EVENT, ev) ;
    resolve_init(wres) ;

    switch (src) {

        case EVENT_SOURCE_INOTIFY:

            if (has_expr || has_tz || has_every) {
                free(wres) ;
                log_warn_return(LOG_EXIT_ZERO, "an inotify source only takes Watch and On of service: ", name) ;
            }
            if (!has_watch || !has_on) {
                free(wres) ;
                log_warn_return(LOG_EXIT_ZERO, "an inotify source requires Watch and On of service: ", name) ;
            }

            v = MAIN_GET(st, WATCH) ;
            if (v[0] != '/') {
                free(wres) ;
                log_warn_return(LOG_EXIT_ZERO, "Watch must be an absolute path: ", v, " of service: ", name) ;
            }
            ev->watch = resolve_add_string(wres, v) ;

            ev->combine = EVENT_COMBINE_ANY ;
            if (!read_list(st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_ON, wres, &ev->on, &ev->non)) {
                free(wres) ;
                log_warnu_return(LOG_EXIT_ZERO, "read the On list of service: ", name) ;
            }

            if (!validate_on_tokens(ev->sa.s + ev->on, EVENT_SOURCE_INOTIFY, 0, 0, name)) {
                free(wres) ;
                return 0 ;
            }
            break ;

        case EVENT_SOURCE_SCHEDULE:
            {

                if (has_watch || has_on || has_every) {
                    free(wres) ;
                    log_warn_return(LOG_EXIT_ZERO, "a schedule source only takes Expression and Timezone of service: ", name) ;
                }
                if (!has_expr) {
                    free(wres) ;
                    log_warn_return(LOG_EXIT_ZERO, "a schedule source requires Expression of service: ", name) ;
                }

                v = MAIN_GET(st, EXPRESSION) ;
                char const *tz = has_tz ? MAIN_GET(st, TIMEZONE) : 0 ;
                cron_t cron = CRON_EXPR_ZERO ;
                if (!parse_cron(v, &cron, tz)) {
                    free(wres) ;
                    log_warnu_return(LOG_EXIT_ZERO, "parse cron Expression: ", v, " of service: ", name) ;
                }

                ev->expression = resolve_add_string(wres, v) ;
                if (has_tz)
                    ev->timezone = resolve_add_string(wres, tz) ;
                break ;
            }

        case EVENT_SOURCE_TIMER:

            if (has_watch || has_on || has_expr || has_tz) {
                free(wres) ;
                log_warn_return(LOG_EXIT_ZERO, "a timer source only takes Every of service: ", name) ;
            }
            if (!has_every) {
                free(wres) ;
                log_warn_return(LOG_EXIT_ZERO, "a timer source requires Every of service: ", name) ;
            }

            v = MAIN_GET(st, EVERY) ;
            if (!every_to_ms(v, &ev->interval)) {
                free(wres) ;
                log_warn_return(LOG_EXIT_ZERO, "invalid Every duration: ", v, " of service: ", name) ;
            }
            break ;

        default:
            break ;
    }

    free(wres) ;

    res->has_event = 1 ;

    return 1 ;
}
