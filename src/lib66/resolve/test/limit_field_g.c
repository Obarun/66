/*
 * limit_field_g.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

/* The generic by-name field API (resolve_get_field / _modify_field_g)
 * must read and write a service's [Limit] addon field: it loads the addon from
 * the core it reads, edits in memory, and persists. An absent addon reads as the
 * default 0. Full on-disk round trip, under ASan/LSan. */

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

static void write_core(char const *base, char const *name, uint32_t has_limit)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_init(w) ;
    res.name = resolve_add_string(w, name) ;
    res.path.home = resolve_add_string(w, base) ;
    res.has_limit = has_limit ;
    assert(resolve_write(w, base, name) > 0) ;
    resolve_free(w) ;
}

static uint64_t get_nofile(char const *base, char const *name)
{
    resolve_enum_table_t t = E_TABLE_SERVICE_LIMIT_ZERO ;
    t.u.service.id = E_RESOLVE_SERVICE_LIMIT_NOFILE ;

    resolve_service_addon_limit_t l = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_LIMIT, &l) ;

    strbuf sa = STRBUF_ZERO ;
    assert(resolve_get_field(&sa, w, base, name, t) > 0) ;
    uint64_t v = 0 ;
    assert(u64_scan_strict(sa.s, &v)) ;
    strbuf_free(&sa) ;
    resolve_free(w) ;
    return v ;
}

int main(void)
{
    printf("Starting limit field _g round-trip test...\n") ;

    char base[64] = "/tmp/66-limfieldg.XXXXXX" ;
    assert(mkdtemp(base)) ;
    strcat(base, "/") ;

    /* a service that carries a limit addon (LimitNOFILE=4096) */
    make_layout(base, "withlim") ;
    write_core(base, "withlim", 1) ;
    {
        resolve_service_addon_limit_t l = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
        resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_LIMIT, &l) ;
        resolve_init(w) ;
        l.limitnofile = 4096 ;
        assert(resolve_write(w, base, "withlim") > 0) ;
        resolve_free(w) ;
    }

    /* read the addon field through the generic by-name path */
    assert(get_nofile(base, "withlim") == 4096) ;

    /* modify it: the addon is a resolve of its own (DATA_SERVICE_LIMIT). The
     * generic by-name path reads .limit, mutates in memory, and writes it back. */
    {
        resolve_service_addon_limit_t l = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
        resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_LIMIT, &l) ;
        resolve_enum_table_t t = E_TABLE_SERVICE_LIMIT_ZERO ;
        t.u.service.id = E_RESOLVE_SERVICE_LIMIT_NOFILE ;
        assert(resolve_modify_field(w, base, "withlim", t, "2048") > 0) ;
        resolve_free(w) ;
    }
    assert(get_nofile(base, "withlim") == 2048) ;

    /* a service with no limit addon: reading a limit field yields the default 0 */
    make_layout(base, "plain") ;
    write_core(base, "plain", 0) ;
    assert(get_nofile(base, "plain") == 0) ;

    /* modify on that service CREATES the addon (ENOENT-tolerant read) and sets
     * the core has_limit (in resolve_write). Reading 512 back proves both the
     * addon on disk and the manifest flag. */
    {
        resolve_service_addon_limit_t l = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
        resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_LIMIT, &l) ;
        resolve_enum_table_t t = E_TABLE_SERVICE_LIMIT_ZERO ;
        t.u.service.id = E_RESOLVE_SERVICE_LIMIT_NOFILE ;
        assert(resolve_modify_field(w, base, "plain", t, "512") > 0) ;
        resolve_free(w) ;
    }
    assert(get_nofile(base, "plain") == 512) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
