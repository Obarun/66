/*
 * event_field_g.c
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

/* The generic by-name field API must read and write a service's event addon
 * field: it loads the addon from the core it reads, edits in memory, persists,
 * and an absent addon reads as the default. Full on-disk round trip, covering a
 * string field (eventfrom), an integer scalar (eventtype) and the
 * create-on-modify path. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/constants.h>
#include <66/enum_service.h>
#include <66/event_rule.h>

static void make_layout(char const *base, char const *name)
{
    char entry[SS_MAX_PATH_LEN], svcdir[SS_MAX_PATH_LEN], svcres[SS_MAX_PATH_LEN], link[SS_MAX_PATH_LEN] ;
    auto_strings(entry, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;
    auto_strings(svcdir, base, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;
    auto_strings(svcres, svcdir, SS_RESOLVE) ;
    auto_strings(link, entry, "/", name) ;

    assert(dir_create_parent(entry, 0755)) ;
    assert(dir_create_parent(svcres, 0755)) ;
    assert(symlink(svcdir, link) == 0) ;
}

static void write_core(char const *base, char const *name, uint32_t has_event)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_init(w) ;
    res.name = resolve_add_string(w, name) ;
    res.path.home = resolve_add_string(w, base) ;
    res.has_event = has_event ;
    assert(resolve_write(w, base, name) > 0) ;
    resolve_free(w) ;
}

static void get_field(strbuf *out, char const *base, char const *name, uint32_t id)
{
    resolve_enum_table_t t = E_TABLE_SERVICE_EVENT_ZERO ;
    t.u.service.id = id ;

    resolve_service_addon_event_t ev = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_EVENT, &ev) ;

    out->len = 0 ;
    assert(resolve_get_field(out, w, base, name, t) > 0) ;
    resolve_free(w) ;
}

static void modify_field(char const *base, char const *name, uint32_t id, char const *val)
{
    resolve_service_addon_event_t ev = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_EVENT, &ev) ;
    resolve_enum_table_t t = E_TABLE_SERVICE_EVENT_ZERO ;
    t.u.service.id = id ;
    assert(resolve_modify_field(w, base, name, t, val) > 0) ;
    resolve_free(w) ;
}

int main(void)
{
    printf("Starting event field _g round-trip test...\n") ;

    char base[64] = "/tmp/66-eventfieldg.XXXXXX" ;
    assert(mkdtemp(base)) ;
    strcat(base, "/") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    /* a service carrying an event addon (eventfrom + neventfrom + eventtype) */
    make_layout(base, "withevent") ;
    write_core(base, "withevent", 1) ;
    {
        resolve_service_addon_event_t ev = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
        resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_EVENT, &ev) ;
        resolve_init(w) ;
        ev.type = EVENT_SOURCE_SERVICE ;
        ev.nfrom = 1 ;
        ev.from = resolve_add_string(w, "alpha") ;
        assert(resolve_write(w, base, "withevent") > 0) ;
        resolve_free(w) ;
    }

    /* eventfrom is a list field: resolve_get_field returns it sbl-cleaned, so a
     * single token round-trips as itself */
    get_field(&sa, base, "withevent", E_RESOLVE_SERVICE_EVENT_FROM) ;
    assert(!strcmp(sa.s, "alpha")) ;

    {
        uint32_t n = 0 ;
        get_field(&sa, base, "withevent", E_RESOLVE_SERVICE_EVENT_TYPE) ;
        assert(u32_scan(sa.s, &n) && n == EVENT_SOURCE_SERVICE) ;
    }

    /* modify the string field through the generic by-name path */
    modify_field(base, "withevent", E_RESOLVE_SERVICE_EVENT_FROM, "beta") ;
    get_field(&sa, base, "withevent", E_RESOLVE_SERVICE_EVENT_FROM) ;
    assert(!strcmp(sa.s, "beta")) ;

    /* a service with no event addon: reading yields the default empty */
    make_layout(base, "plain") ;
    write_core(base, "plain", 0) ;
    get_field(&sa, base, "plain", E_RESOLVE_SERVICE_EVENT_FROM) ;
    assert(sa.s[0] == 0) ;

    /* modify on that service CREATES the addon and stores a string at offset 0
     * safely (create-on-modify resolve_init guard) */
    modify_field(base, "plain", E_RESOLVE_SERVICE_EVENT_WATCH, "/etc/resolv.conf") ;
    get_field(&sa, base, "plain", E_RESOLVE_SERVICE_EVENT_WATCH) ;
    assert(!strcmp(sa.s, "/etc/resolv.conf")) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
