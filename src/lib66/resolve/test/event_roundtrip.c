/*
 * event_roundtrip.c
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

/* Round-trip the autonomous `event` addon CDB: fill every leaf (the string
 * offsets and the integer scalars) in memory, write it as its own CDB, read it
 * back and assert every field survives. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/event_rule.h>

static void test_roundtrip(char const *dir)
{
    printf("Running event addon round-trip\n") ;

    resolve_service_addon_event_t ev = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_EVENT, &ev) ;
    resolve_init(w) ;

    ev.type = EVENT_SOURCE_SCHEDULE ;
    ev.from = resolve_add_string(w, "svc1 svc2") ;
    ev.nfrom = 2 ;
    ev.fromfield = EVENT_FROMFIELD_DEPENDS | EVENT_FROMFIELD_REQUIREDBY ;
    ev.on = resolve_add_string(w, "up down") ;
    ev.non = 2 ;
    ev.combine = EVENT_COMBINE_ALL ;
    ev.docmd = EVENT_DO_RESTART ;
    ev.emit = resolve_add_string(w, "cert-renewed") ;
    ev.watch = resolve_add_string(w, "/etc/resolv.conf") ;
    ev.expression = resolve_add_string(w, "0 0 3 * * ?") ;
    ev.timezone = resolve_add_string(w, "Europe/Paris") ;
    ev.interval = 60000 ;

    if (!resolve_write_cdb(w, dir, "svc.event")) {
        printf("FAIL: resolve_write_cdb\n") ;
        assert(0) ;
    }

    resolve_service_addon_event_t back = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t *wback = resolve_set_struct(DATA_SERVICE_EVENT, &back) ;
    int r = resolve_read_cdb(wback, dir, "svc.event") ;
    if (r <= 0) {
        printf("FAIL: resolve_read_cdb returned %d\n", r) ;
        assert(0) ;
    }

    assert(back.type == EVENT_SOURCE_SCHEDULE) ;
    assert(!strcmp(back.sa.s + back.from, "svc1 svc2")) ;
    assert(back.nfrom == 2) ;
    assert(back.fromfield == (EVENT_FROMFIELD_DEPENDS | EVENT_FROMFIELD_REQUIREDBY)) ;
    assert(!strcmp(back.sa.s + back.on, "up down")) ;
    assert(back.non == 2) ;
    assert(back.combine == EVENT_COMBINE_ALL) ;
    assert(back.docmd == EVENT_DO_RESTART) ;
    assert(!strcmp(back.sa.s + back.emit, "cert-renewed")) ;
    assert(!strcmp(back.sa.s + back.watch, "/etc/resolv.conf")) ;
    assert(!strcmp(back.sa.s + back.expression, "0 0 3 * * ?")) ;
    assert(!strcmp(back.sa.s + back.timezone, "Europe/Paris")) ;
    assert(back.interval == 60000) ;

    /* the sanitize path must preserve every string leaf too */
    service_resolve_sanitize_addon_event(&back) ;
    assert(!strcmp(back.sa.s + back.from, "svc1 svc2")) ;
    assert(!strcmp(back.sa.s + back.on, "up down")) ;
    assert(!strcmp(back.sa.s + back.emit, "cert-renewed")) ;
    assert(!strcmp(back.sa.s + back.watch, "/etc/resolv.conf")) ;
    assert(!strcmp(back.sa.s + back.expression, "0 0 3 * * ?")) ;
    assert(!strcmp(back.sa.s + back.timezone, "Europe/Paris")) ;
    assert(back.type == EVENT_SOURCE_SCHEDULE) ;
    assert(back.interval == 60000) ;

    resolve_free(wback) ;
    resolve_free(w) ;

    printf(" PASS\n") ;
}

int main(void)
{
    printf("Starting event addon round-trip test...\n") ;

    char dir[] = "/tmp/66-event-test.XXXXXX" ;
    if (!mkdtemp(dir)) {
        printf("FAIL: mkdtemp\n") ;
        return 1 ;
    }
    size_t dlen = strlen(dir) ;
    char path[dlen + 2] ;
    memcpy(path, dir, dlen) ;
    path[dlen] = '/' ;
    path[dlen + 1] = 0 ;

    test_roundtrip(path) ;

    char file[dlen + 1 + 10] ;
    memcpy(file, dir, dlen) ;
    memcpy(file + dlen, "/svc.event", 11) ;
    unlink(file) ;
    rmdir(dir) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
