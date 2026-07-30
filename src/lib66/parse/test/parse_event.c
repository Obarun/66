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
#include <stddef.h> // offsetof
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <oblibs/hash.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h> // auto_strbuf

#include <66/parse.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/event_rule.h>

// holds no resolve file: the disk branch of the From check must reject, not crash
#define TEST_BASE "/nonexistent-66-parse-event-base"

/** the environment parse_event reads a tick reactor's From source from: the
 * batch hash first, the resolve files of an earlier run otherwise. */
typedef struct { hash_t hres ; ssexec_t info ; } test_env_t ;

static void env_start(test_env_t *e)
{
    e->hres = (hash_t)HASH_ZERO ;
    assert(hash_init(&e->hres, 0, offsetof(struct resolve_hash_s, node)) == 1) ;

    e->info = (ssexec_t)SSEXEC_ZERO ;
    // auto_strbuf, like set_ownersysdir: base.s must be a NUL-terminated path
    assert(auto_strbuf(&e->info.base, TEST_BASE) == 1) ;
}

static void env_free(test_env_t *e)
{
    resolve_hash_free(&e->hres) ;
    strbuf_free(&e->info.base) ;
}

/** seed the hash with what parse_interdependences leaves behind: the From target,
 * already built, with its service type and event family set */
static void env_add(test_env_t *e, char const *name, uint32_t svtype, uint32_t evtype)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    res.type = svtype ;
    res.has_event = 1 ;

    assert(resolve_hash_add(&e->hres, name, res) == 1) ;

    struct resolve_hash_s *h = resolve_hash_search(&e->hres, name) ;
    assert(h != NULL) ;

    h->event.type = evtype ;
}

static int run_env(char const *fe, struct resolve_hash_s *c, resolve_wrapper_t **w, parse_store_t *st, test_env_t *e)
{
    *c = (struct resolve_hash_s){0} ;
    assert(parse_store_build(st, fe) == 1) ;
    *w = resolve_set_struct(DATA_SERVICE, &c->res) ;
    resolve_init(*w) ;
    c->res.name = resolve_add_string(*w, "testsvc") ;
    parse_build_ctx_t ctx = { .st = st, .conf = 0, .hres = &e->hres, .info = &e->info } ;
    return parse_event(c, &ctx) ;
}

static int run(char const *fe, struct resolve_hash_s *c, resolve_wrapper_t **w, parse_store_t *st)
{
    test_env_t e ;
    env_start(&e) ;

    int r = run_env(fe, c, w, st, &e) ;

    env_free(&e) ;

    return r ;
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
    assert(!strcmp(c.event.sa.s + c.event.on, "down")) ;
    assert(c.event.non == 1) ;
    assert(c.event.combine == EVENT_COMBINE_ANY) ;
    assert(c.event.docmd == EVENT_DO_RESTART) ;
    assert(c.event.emit == 0) ;

    cleanup(&c, w, &st) ;
}

/* signal reactor: OnAll (all) + Emit, explicit From */
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
        "From = ( db )\n"
        "OnAll = ( SIGHUP SIGTERM )\n"
        "Emit = sig-caught\n" ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run(fe, &c, &w, &st) == 1) ;

    assert(c.res.has_event == 1) ;
    assert(c.event.type == EVENT_SOURCE_SIGNAL) ;
    assert(!strcmp(c.event.sa.s + c.event.from, "db")) ;
    assert(c.event.nfrom == 1) ;
    assert(!strcmp(c.event.sa.s + c.event.on, "SIGHUP SIGTERM")) ;
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

    test_env_t e ;
    env_start(&e) ;
    env_add(&e, "resolv-watch", E_PARSER_TYPE_EVENT, EVENT_SOURCE_INOTIFY) ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run_env(fe, &c, &w, &st, &e) == 1) ;

    assert(c.res.has_event == 1) ;
    assert(c.event.type == EVENT_SOURCE_INOTIFY) ;
    assert(!strcmp(c.event.sa.s + c.event.from, "resolv-watch")) ;
    assert(c.event.nfrom == 1) ;
    assert(c.event.on == 0 && c.event.non == 0) ;
    assert(c.event.docmd == EVENT_DO_RELOAD) ;

    cleanup(&c, w, &st) ;
    env_free(&e) ;
}

// a tick reactor accepts several sources of its own family
static void schedule_from_multi(void)
{
    printf("Running test schedule_from_multi...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = oneshot\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "[Event]\n"
        "EventType = schedule\n"
        "From = ( nightly weekly )\n"
        "Do = start\n" ;

    test_env_t e ;
    env_start(&e) ;
    env_add(&e, "nightly", E_PARSER_TYPE_EVENT, EVENT_SOURCE_SCHEDULE) ;
    env_add(&e, "weekly", E_PARSER_TYPE_EVENT, EVENT_SOURCE_SCHEDULE) ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run_env(fe, &c, &w, &st, &e) == 1) ;

    assert(c.event.type == EVENT_SOURCE_SCHEDULE) ;
    assert(!strcmp(c.event.sa.s + c.event.from, "nightly weekly")) ;
    assert(c.event.nfrom == 2) ;

    cleanup(&c, w, &st) ;
    env_free(&e) ;
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

// rejections that need a seeded From target in the hash
static void rejected_from(char const *label, char const *fe, char const *src, uint32_t svtype, uint32_t evtype)
{
    printf("Running From rejection test %s...\n", label) ;

    test_env_t e ;
    env_start(&e) ;
    env_add(&e, src, svtype, evtype) ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run_env(fe, &c, &w, &st, &e) == 0) ;

    cleanup(&c, w, &st) ;
    env_free(&e) ;
}

/* service On predicates: status state|result words, svc:cond (svc in From),
 * exited:<code>, signaled:<SIG> */
static void service_on_predicates(void)
{
    printf("Running test service_on_predicates...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "[Event]\n"
        "EventType = service\n"
        "From = ( db web )\n"
        "On = ( down signaled db:exited:5 web:up signaled:SIGKILL exec-failed )\n"
        "Do = restart\n" ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run(fe, &c, &w, &st) == 1) ;

    assert(c.res.has_event == 1) ;
    assert(!strcmp(c.event.sa.s + c.event.on, "down signaled db:exited:5 web:up signaled:SIGKILL exec-failed")) ;
    assert(c.event.non == 6) ;

    cleanup(&c, w, &st) ;
}

static int run_source(char const *fe, struct resolve_hash_s *c, resolve_wrapper_t **w, parse_store_t *st)
{
    *c = (struct resolve_hash_s){0} ;
    assert(parse_store_build(st, fe) == 1) ;
    *w = resolve_set_struct(DATA_SERVICE, &c->res) ;
    resolve_init(*w) ;
    c->res.name = resolve_add_string(*w, "testsrc") ;
    parse_build_ctx_t ctx = { .st = st, .conf = 0 } ;
    return parse_event_source(c, &ctx) ;
}

/* inotify source: Watch + On (IN_* verbatim) */
static void source_inotify(void)
{
    printf("Running test source_inotify...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = event\n"
        "EventType = inotify\n"
        "Watch = /etc/resolv.conf\n"
        "On = ( IN_MODIFY IN_CREATE )\n" ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run_source(fe, &c, &w, &st) == 1) ;

    assert(c.res.has_event == 1) ;
    assert(c.event.type == EVENT_SOURCE_INOTIFY) ;
    assert(!strcmp(c.event.sa.s + c.event.watch, "/etc/resolv.conf")) ;
    assert(!strcmp(c.event.sa.s + c.event.on, "IN_MODIFY IN_CREATE")) ;
    assert(c.event.non == 2) ;
    assert(c.event.combine == EVENT_COMBINE_ANY) ;

    cleanup(&c, w, &st) ;
}

/* schedule source: cron Expression + Timezone */
static void source_schedule(void)
{
    printf("Running test source_schedule...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = event\n"
        "EventType = schedule\n"
        "Expression = \"0 0 3 * * ?\"\n"
        "Timezone = Europe/Paris\n" ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run_source(fe, &c, &w, &st) == 1) ;

    assert(c.res.has_event == 1) ;
    assert(c.event.type == EVENT_SOURCE_SCHEDULE) ;
    assert(!strcmp(c.event.sa.s + c.event.expression, "0 0 3 * * ?")) ;
    assert(!strcmp(c.event.sa.s + c.event.timezone, "Europe/Paris")) ;

    cleanup(&c, w, &st) ;
}

/* timer source: Every with a suffix, normalised to milliseconds */
static void source_timer(void)
{
    printf("Running test source_timer...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = event\n"
        "EventType = timer\n"
        "Every = 5m\n" ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run_source(fe, &c, &w, &st) == 1) ;

    assert(c.res.has_event == 1) ;
    assert(c.event.type == EVENT_SOURCE_TIMER) ;
    assert(c.event.interval == 300000) ; // 5 * 60 * 1000

    cleanup(&c, w, &st) ;
}

/* Every defaults to seconds */
static void source_timer_default(void)
{
    printf("Running test source_timer_default...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = event\n"
        "EventType = timer\n"
        "Every = 30\n" ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run_source(fe, &c, &w, &st) == 1) ;

    assert(c.event.interval == 30000) ;

    cleanup(&c, w, &st) ;
}

static void rejected_source(char const *label, char const *fe)
{
    printf("Running source rejection test %s...\n", label) ;

    struct resolve_hash_s c ; resolve_wrapper_t *w ; parse_store_t st ;
    assert(run_source(fe, &c, &w, &st) == 0) ;

    cleanup(&c, w, &st) ;
}

int main(void)
{
    service_on_do() ;
    service_on_predicates() ;
    signal_onall_emit() ;
    user_on() ;
    inotify_from_do() ;
    schedule_from_multi() ;
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

    rejected("missing From",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = service\nOn = ( down )\nDo = restart\n") ;

    rejected("bad Do",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = service\nFrom = ( db )\nOn = ( down )\nDo = explode\n") ;

    rejected("bad On predicate",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = service\nFrom = ( db )\nOn = ( bogus )\nDo = restart\n") ;

    rejected("svc not in From",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = service\nFrom = ( db )\nOn = ( other:up )\nDo = restart\n") ;

    rejected("exited non-numeric",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = service\nFrom = ( db )\nOn = ( exited:abc )\nDo = restart\n") ;

    rejected("signaled bad signal",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = service\nFrom = ( db )\nOn = ( signaled:NOSIG )\nDo = restart\n") ;

    rejected("bad signal name",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = signal\nFrom = ( db )\nOn = ( NOTASIGNAL )\nDo = restart\n") ;

    // the From source is neither in the batch nor on disk
    rejected("unknown tick source",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = timer\nFrom = ( ghost )\nDo = start\n") ;

    rejected_from("EventType against its source family",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = timer\nFrom = ( nightly )\nDo = start\n",
        "nightly", E_PARSER_TYPE_EVENT, EVENT_SOURCE_SCHEDULE) ;

    // the event family agrees: only the service type tells a reactor from a source
    rejected_from("From is a reactor, not a source",
        "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n"
        "[Event]\nEventType = inotify\nFrom = ( peer )\nDo = restart\n",
        "peer", E_PARSER_TYPE_CLASSIC, EVENT_SOURCE_INOTIFY) ;

    source_inotify() ;
    source_schedule() ;
    source_timer() ;
    source_timer_default() ;

    rejected_source("non-producer EventType",
        "[Main]\nType = event\nEventType = service\nOn = ( down )\n") ;

    rejected_source("inotify without On",
        "[Main]\nType = event\nEventType = inotify\nWatch = /etc/resolv.conf\n") ;

    rejected_source("inotify with foreign key",
        "[Main]\nType = event\nEventType = inotify\nWatch = /x\nOn = ( IN_MODIFY )\nEvery = 5\n") ;

    rejected_source("relative Watch",
        "[Main]\nType = event\nEventType = inotify\nWatch = etc/resolv.conf\nOn = ( IN_MODIFY )\n") ;

    rejected_source("bad cron",
        "[Main]\nType = event\nEventType = schedule\nExpression = \"not a cron\"\n") ;

    rejected_source("timer bad Every",
        "[Main]\nType = event\nEventType = timer\nEvery = 5x\n") ;

    rejected_source("bad inotify constant",
        "[Main]\nType = event\nEventType = inotify\nWatch = /x\nOn = ( IN_BOGUS )\n") ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
