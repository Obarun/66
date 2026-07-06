/*
 * environ_field_g.c
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

/* The generic by-name field API must read and write a service's environ addon
 * field: it loads the addon from the core it reads, edits in memory, persists,
 * and an absent addon reads as the default (empty). Full on-disk round trip. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/strbuf.h>

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

static void write_core(char const *base, char const *name, uint32_t has_environ)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_init(w) ;
    res.name = resolve_add_string(w, name) ;
    res.path.home = resolve_add_string(w, base) ;
    res.has_environ = has_environ ;
    assert(resolve_write(w, base, name) > 0) ;
    resolve_free(w) ;
}

/* read a string environ field into @out (caller frees), asserting success */
static void get_field(strbuf *out, char const *base, char const *name, uint32_t id)
{
    resolve_enum_table_t t = E_TABLE_SERVICE_ENVIRON_ZERO ;
    t.u.service.id = id ;

    resolve_service_addon_environ_t e = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_ENVIRON, &e) ;

    out->len = 0 ;
    assert(resolve_get_field(out, w, base, name, t) > 0) ;
    resolve_free(w) ;
}

int main(void)
{
    printf("Starting environ field _g round-trip test...\n") ;

    char base[64] = "/tmp/66-envfieldg.XXXXXX" ;
    assert(mkdtemp(base)) ;
    strcat(base, "/") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    /* a service carrying an environ addon (envdir set) */
    make_layout(base, "withenv") ;
    write_core(base, "withenv", 1) ;
    {
        resolve_service_addon_environ_t e = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
        resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_ENVIRON, &e) ;
        resolve_init(w) ;
        e.envdir = resolve_add_string(w, "/etc/66/conf/withenv") ;
        assert(resolve_write(w, base, "withenv") > 0) ;
        resolve_free(w) ;
    }

    get_field(&sa, base, "withenv", E_RESOLVE_SERVICE_ENVIRON_ENVDIR) ;
    assert(!strcmp(sa.s, "/etc/66/conf/withenv")) ;

    /* modify the string field through the generic by-name path */
    {
        resolve_service_addon_environ_t e = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
        resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_ENVIRON, &e) ;
        resolve_enum_table_t t = E_TABLE_SERVICE_ENVIRON_ZERO ;
        t.u.service.id = E_RESOLVE_SERVICE_ENVIRON_ENVDIR ;
        assert(resolve_modify_field(w, base, "withenv", t, "/etc/66/conf/moved") > 0) ;
        resolve_free(w) ;
    }
    get_field(&sa, base, "withenv", E_RESOLVE_SERVICE_ENVIRON_ENVDIR) ;
    assert(!strcmp(sa.s, "/etc/66/conf/moved")) ;

    /* a service with no environ addon: reading a field yields the default empty */
    make_layout(base, "plain") ;
    write_core(base, "plain", 0) ;
    get_field(&sa, base, "plain", E_RESOLVE_SERVICE_ENVIRON_ENVDIR) ;
    assert(sa.s[0] == 0) ;

    /* modify on that service CREATES the addon and sets core has_environ */
    {
        resolve_service_addon_environ_t e = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
        resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_ENVIRON, &e) ;
        resolve_enum_table_t t = E_TABLE_SERVICE_ENVIRON_ZERO ;
        t.u.service.id = E_RESOLVE_SERVICE_ENVIRON_ENVDIR ;
        assert(resolve_modify_field(w, base, "plain", t, "/etc/66/conf/plain") > 0) ;
        resolve_free(w) ;
    }
    get_field(&sa, base, "plain", E_RESOLVE_SERVICE_ENVIRON_ENVDIR) ;
    assert(!strcmp(sa.s, "/etc/66/conf/plain")) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
