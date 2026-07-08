/*
 * parse_event.c -- unit test for the store-based reactor [Event] parser
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

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <oblibs/strbuf.h>

#include <66/parse.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/event_rule.h>

static int run(char const *fe, struct resolve_hash_s *c, resolve_wrapper_t **w, parse_store_t *st)
{
    *c = (struct resolve_hash_s){0} ;
    assert(parse_store_build(st, fe) == 1) ;
    *w = resolve_set_struct(DATA_SERVICE, &c->res) ;
    resolve_init(*w) ;
    c->res.name = resolve_add_string(*w, "testsvc") ;
    parse_build_ctx_t ctx = { .st = st, .conf = 0 } ;
    return parse_event(c, &ctx) ;
}

static void cleanup(struct resolve_hash_s *c, resolve_wrapper_t *w, parse_store_t *st)
{
    strbuf_free(&c->event.sa) ;
    resolve_free(w) ;
    parse_store_free(st) ;
}

/* service reactor: On (any) + Do, single From */
static void service_on_do(void)
{
    printf("Running test service_on_do...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "[Event]\n"
        "EventType = service\n"
        "From = ( db )\n"
        "On = ( down )\n"
        "Do = restart\n" ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run(fe, &c, &w, &st) == 1) ;

    assert(c.res.has_event == 1) ;
    assert(c.event.type == EVENT_SOURCE_SERVICE) ;
    assert(!strcmp(c.event.sa.s + c.event.from, "db")) ;
    assert(c.event.nfrom == 1) ;
    assert(c.event.fromfield == 0) ;
    assert(!strcmp(c.event.sa.s + c.event.on, "down")) ;
    assert(c.event.non == 1) ;
    assert(c.event.combine == EVENT_COMBINE_ANY) ;
    assert(c.event.docmd == EVENT_DO_RESTART) ;
    assert(c.event.emit == 0) ;

    cleanup(&c, w, &st) ;
}

/* signal reactor: OnAll (all) + Emit, FromField=Depends */
static void signal_onall_emit(void)
{
    printf("Running test signal_onall_emit...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "[Event]\n"
        "EventType = signal\n"
        "FromField = Depends\n"
        "OnAll = ( up ready )\n"
        "Emit = sig-caught\n" ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run(fe, &c, &w, &st) == 1) ;

    assert(c.res.has_event == 1) ;
    assert(c.event.type == EVENT_SOURCE_SIGNAL) ;
    assert(c.event.from == 0 && c.event.nfrom == 0) ;
    assert(c.event.fromfield == EVENT_FROMFIELD_DEPENDS) ;
    assert(!strcmp(c.event.sa.s + c.event.on, "up ready")) ;
    assert(c.event.non == 2) ;
    assert(c.event.combine == EVENT_COMBINE_ALL) ;
    assert(c.event.docmd == EVENT_DO_NONE) ;
    assert(!strcmp(c.event.sa.s + c.event.emit, "sig-caught")) ;

    cleanup(&c, w, &st) ;
}

/* user reactor: On only, no source */
static void user_on(void)
{
    printf("Running test user_on...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "[Event]\n"
        "EventType = user\n"
        "On = ( mytrigger )\n"
        "Do = start\n" ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run(fe, &c, &w, &st) == 1) ;

    assert(c.res.has_event == 1) ;
    assert(c.event.type == EVENT_SOURCE_USER) ;
    assert(c.event.from == 0 && c.event.nfrom == 0) ;
    assert(!strcmp(c.event.sa.s + c.event.on, "mytrigger")) ;
    assert(c.event.combine == EVENT_COMBINE_ANY) ;
    assert(c.event.docmd == EVENT_DO_START) ;

    cleanup(&c, w, &st) ;
}

/* inotify reactor: From + Do, no On/OnAll (condition in the source) */
static void inotify_from_do(void)
{
    printf("Running test inotify_from_do...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "[Event]\n"
        "EventType = inotify\n"
        "From = ( resolv-watch )\n"
        "Do = reload\n" ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run(fe, &c, &w, &st) == 1) ;

    assert(c.res.has_event == 1) ;
    assert(c.event.type == EVENT_SOURCE_INOTIFY) ;
    assert(!strcmp(c.event.sa.s + c.event.from, "resolv-watch")) ;
    assert(c.event.nfrom == 1) ;
    assert(c.event.on == 0 && c.event.non == 0) ;
    assert(c.event.docmd == EVENT_DO_RELOAD) ;

    cleanup(&c, w, &st) ;
}

/* no [Event] section -> no rule, has_event stays cleared */
static void without_event(void)
{
    printf("Running test without_event...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n" ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    c = (struct resolve_hash_s){0} ;
    assert(parse_store_build(&st, fe) == 1) ;
    w = resolve_set_struct(DATA_SERVICE, &c.res) ;
    resolve_init(w) ;
    c.res.name = resolve_add_string(w, "testsvc") ;
    c.res.has_event = 1 ; // seed non-zero to prove parse_event clears it
    parse_build_ctx_t ctx = { .st = &st, .conf = 0 } ;
    assert(parse_event(&c, &ctx) == 1) ;

    assert(c.res.has_event == 0) ;
    assert(c.event.type == 0 && c.event.from == 0 && c.event.on == 0) ;

    cleanup(&c, w, &st) ;
}

/* rejections: each must make parse_event return 0 */
static void rejected(char const *label, char const *fe)
{
    printf("Running rejection test %s...\n", label) ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run(fe, &c, &w, &st) == 0) ;

    cleanup(&c, w, &st) ;
}

int main(void)
{
    service_on_do() ;
    signal_onall_emit() ;
    user_on() ;
    inotify_from_do() ;
    without_event() ;

    rejected("no source",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = service\nOn = ( down )\nDo = restart\n") ;

    rejected("On and OnAll",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = service\nFrom = ( db )\nOn = ( down )\nOnAll = ( up )\nDo = restart\n") ;

    rejected("user with source",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = user\nFrom = ( db )\nOn = ( x )\nDo = start\n") ;

    rejected("inotify with On",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = inotify\nFrom = ( w )\nOn = ( down )\nDo = reload\n") ;

    rejected("no action",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = service\nFrom = ( db )\nOn = ( down )\n") ;

    rejected("bad EventType",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = bogus\nFrom = ( db )\nOn = ( down )\nDo = restart\n") ;

    rejected("bad FromField",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = service\nFromField = Nope\nOn = ( down )\nDo = restart\n") ;

    rejected("bad Do",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = service\nFrom = ( db )\nOn = ( down )\nDo = explode\n") ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
