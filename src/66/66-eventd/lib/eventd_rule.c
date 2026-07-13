/*
 * event_rule.c
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
#include <stdlib.h> // free

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h>

#include "eventd.h"
#include <66/resolve.h>
#include <66/status.h>

int eventd_rule_load(char const *base, char const *name, resolve_service_addon_event_t *out)
{
    log_flow() ;

    *out = (resolve_service_addon_event_t)RESOLVE_SERVICE_ADDON_EVENT_ZERO ;

    resolve_wrapper_t_ref wev = resolve_set_struct(DATA_SERVICE_EVENT, out) ;
    int r = resolve_read(wev, base, name) ;
    free(wev) ;

    return r ; // 1 = rule loaded, 0 = no .event addon, -1 = error
}

void eventd_rule_free(resolve_service_addon_event_t *r)
{
    log_flow() ;

    if (!r)
        return ;

    strbuf_free(&r->sa) ;
    *r = (resolve_service_addon_event_t)RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
}

int eventd_token_match(char const *token, event_frame_t const *f)
{
    // a signal rule: the token is a SIGxxx (name or number), matched on a SIGNAL frame
    if (f->kind == EVENT_KIND_SIGNAL) {
        int sig ;
        return sig_parse(token, &sig) && f->signo == (uint8_t)sig ;
    }

    // only a state transition carries the service On vocabulary
    if (f->kind != EVENT_KIND_TRANSITION)
        return 0 ;

    char const *colon = strchr(token, ':') ;

    if (!colon) {
        int s = status_state_from_string(token) ;
        if (s >= 0)
            return f->state == (uint8_t)s ;
        int r = status_result_from_string(token) ;
        if (r >= 0)
            return f->result == (uint8_t)r ;
        return 0 ; // unknown token
    }

    // argument predicate: <result-word>:<arg>
    size_t wlen = (size_t)(colon - token) ;
    char word[wlen + 1] ;
    memcpy(word, token, wlen) ;
    word[wlen] = 0 ;
    char const *arg = colon + 1 ;

    switch (status_result_from_string(word)) {

        case STATUS_RESULT_EXITED : {
            uint32_t n ;
            return u32_scan(arg, &n) && f->result == STATUS_RESULT_EXITED && f->code == n ;
        }

        case STATUS_RESULT_SIGNALED : {
            int sig ;
            if (!sig_parse(arg, &sig)) {
                uint32_t n ;
                if (!u32_scan(arg, &n)) return 0 ; // neither a signal name nor a number
                sig = (int)n ;
            }
            return f->result == STATUS_RESULT_SIGNALED && f->code == (uint32_t)sig ;
        }

        default :
            return 0 ; // any other word does not take an argument
    }
}

static char const *list_next(char const *p, size_t *len)
{
    ssize_t n = get_len_until(p, ' ') ;
    if (n < 0) {
        *len = strlen(p) ; // last entry: no trailing space
        return p + *len ;
    }
    *len = (size_t)n ;
    return p + n + 1 ;
}

static int source_in_from(resolve_service_addon_event_t const *r, char const *svc, size_t len)
{
    char const *p = r->sa.s + r->from ;
    for (uint32_t i = 0 ; i < r->nfrom ; i++) {
        size_t plen ;
        char const *next = list_next(p, &plen) ;
        if (plen == len && !memcmp(p, svc, len))
            return 1 ;
        p = next ;
    }
    return 0 ;
}

int eventd_rule_match(resolve_service_addon_event_t const *r, char const *source, event_frame_t const *f)
{
    if (!r->non)
        return 0 ;

    size_t srclen = strlen(source) ;
    int any = 0, all = 1, napplicable = 0 ;
    char const *p = r->sa.s + r->on ;

    for (uint32_t i = 0 ; i < r->non ; i++) {

        size_t tlen ;
        char const *start = p ;
        p = list_next(p, &tlen) ;

        char token[tlen + 1] ; // the on list is space-separated, entries are not NUL-terminated
        memcpy(token, start, tlen) ;
        token[tlen] = 0 ;

        char const *cond = token ;
        char const *colon = strchr(token, ':') ;

        if (colon && source_in_from(r, token, (size_t)(colon - token))) {
            // a `svc:cond` per-source token: applies only when svc == source
            size_t svclen = (size_t)(colon - token) ;
            if (svclen != srclen || memcmp(token, source, srclen))
                continue ;
            cond = colon + 1 ;
        }
        // otherwise a bare token (or an argument token like exit:0): applies here

        napplicable++ ;
        int m = eventd_token_match(cond, f) ;
        any = any || m ;
        all = all && m ;
    }

    if (!napplicable)
        return 0 ;

    // ALL is completed cross-source by the daemon; here it is this source's part
    return r->combine == EVENT_COMBINE_ALL ? all : any ;
}

int eventd_rule_onall(resolve_service_addon_event_t const *r, char const *source, eventd_frame_fn get_frame, void *ctx)
{
    if (r->combine != EVENT_COMBINE_ALL)
        return 1 ;

    size_t srclen = strlen(source) ;
    char const *p = r->sa.s + r->on ;

    for (uint32_t i = 0 ; i < r->non ; i++) {

        size_t tlen ;
        char const *start = p ;
        p = list_next(p, &tlen) ;

        char token[tlen + 1] ;
        memcpy(token, start, tlen) ;
        token[tlen] = 0 ;

        char const *colon = strchr(token, ':') ;
        if (!colon)
            continue ; // bare token: decided against the incoming frame in eventd_rule_match

        size_t svclen = (size_t)(colon - token) ;
        if (!source_in_from(r, token, svclen))
            continue ; // the colon is an argument (e.g. exited:0), not a source prefix

        if (svclen == srclen && !memcmp(token, source, srclen))
            continue ; // this frame's own source: already confirmed

        event_frame_t f ;
        if (!get_frame(token, svclen, &f, ctx))
            return 0 ; // the other source's state is unknown -> conjunction cannot hold

        if (!eventd_token_match(colon + 1, &f))
            return 0 ;
    }

    return 1 ;
}
