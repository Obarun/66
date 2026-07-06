/*
 * regex_field_g.c
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

/* The generic by-name field API must read and write a service's regex addon
 * field: it loads the addon from the core it reads, edits in memory, persists,
 * and an absent addon reads as the default. Full on-disk round trip, covering a
 * scalar string field (configure), a list field (directories) and an integer
 * count (ndirectories). */

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

static void write_core(char const *base, char const *name, uint32_t has_regex)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_init(w) ;
    res.name = resolve_add_string(w, name) ;
    res.path.home = resolve_add_string(w, base) ;
    res.has_regex = has_regex ;
    assert(resolve_write(w, base, name) > 0) ;
    resolve_free(w) ;
}

static void get_field(strbuf *out, char const *base, char const *name, uint32_t id)
{
    resolve_enum_table_t t = E_TABLE_SERVICE_REGEX_ZERO ;
    t.u.service.id = id ;

    resolve_service_addon_regex_t rx = RESOLVE_SERVICE_ADDON_REGEX_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_REGEX, &rx) ;

    out->len = 0 ;
    assert(resolve_get_field(out, w, base, name, t) > 0) ;
    resolve_free(w) ;
}

static void modify_field(char const *base, char const *name, uint32_t id, char const *val)
{
    resolve_service_addon_regex_t rx = RESOLVE_SERVICE_ADDON_REGEX_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_REGEX, &rx) ;
    resolve_enum_table_t t = E_TABLE_SERVICE_REGEX_ZERO ;
    t.u.service.id = id ;
    assert(resolve_modify_field(w, base, name, t, val) > 0) ;
    resolve_free(w) ;
}

int main(void)
{
    printf("Starting regex field _g round-trip test...\n") ;

    char base[64] = "/tmp/66-regexfieldg.XXXXXX" ;
    assert(mkdtemp(base)) ;
    strcat(base, "/") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    /* a service carrying a regex addon (configure + directories + ndirectories) */
    make_layout(base, "withregex") ;
    write_core(base, "withregex", 1) ;
    {
        resolve_service_addon_regex_t rx = RESOLVE_SERVICE_ADDON_REGEX_ZERO ;
        resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_REGEX, &rx) ;
        resolve_init(w) ;
        rx.configure = resolve_add_string(w, "setup.sh") ;
        rx.directories = resolve_add_string(w, "alpha") ;
        rx.ndirectories = 1 ;
        assert(resolve_write(w, base, "withregex") > 0) ;
        resolve_free(w) ;
    }

    /* configure is a scalar string: it round-trips as itself */
    get_field(&sa, base, "withregex", E_RESOLVE_SERVICE_REGEX_CONFIGURE) ;
    assert(!strcmp(sa.s, "setup.sh")) ;

    /* directories is a list field: resolve_get_field returns it sbl-cleaned, so a
     * single token round-trips as itself */
    get_field(&sa, base, "withregex", E_RESOLVE_SERVICE_REGEX_DIRECTORIES) ;
    assert(!strcmp(sa.s, "alpha")) ;

    {
        uint32_t n = 0 ;
        get_field(&sa, base, "withregex", E_RESOLVE_SERVICE_REGEX_NDIRECTORIES) ;
        assert(u32_scan(sa.s, &n) && n == 1) ;
    }

    /* modify the scalar field through the generic by-name path */
    modify_field(base, "withregex", E_RESOLVE_SERVICE_REGEX_CONFIGURE, "other.sh") ;
    get_field(&sa, base, "withregex", E_RESOLVE_SERVICE_REGEX_CONFIGURE) ;
    assert(!strcmp(sa.s, "other.sh")) ;

    /* a service with no regex addon: reading yields the default empty */
    make_layout(base, "plain") ;
    write_core(base, "plain", 0) ;
    get_field(&sa, base, "plain", E_RESOLVE_SERVICE_REGEX_CONFIGURE) ;
    assert(sa.s[0] == 0) ;

    /* modify on that service CREATES the addon and stores a string at offset 0
     * safely (regression guard for the create-on-modify resolve_init fix) */
    modify_field(base, "plain", E_RESOLVE_SERVICE_REGEX_FILES, "freshfile") ;
    get_field(&sa, base, "plain", E_RESOLVE_SERVICE_REGEX_FILES) ;
    assert(!strcmp(sa.s, "freshfile")) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
