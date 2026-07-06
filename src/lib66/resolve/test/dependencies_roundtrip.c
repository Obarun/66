/*
 * dependencies_roundtrip.c
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

/* Round-trip the autonomous `dependencies` addon CDB: build the six relation
 * lists and their counts in memory, write it as its own CDB, read it back and
 * assert every leaf survives. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <66/resolve.h>
#include <66/service.h>

static void test_roundtrip(char const *dir)
{
    printf("Running dependencies addon round-trip\n") ;

    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &dep) ;
    resolve_init(w) ;

    dep.depends = resolve_add_string(w, "dep1 dep2") ;
    dep.ndepends = 2 ;
    dep.requiredby = resolve_add_string(w, "svc-log") ;
    dep.nrequiredby = 1 ;
    dep.optsdeps = resolve_add_string(w, "opt1") ;
    dep.noptsdeps = 1 ;
    dep.contents = resolve_add_string(w, "a b c") ;
    dep.ncontents = 3 ;
    dep.provide = resolve_add_string(w, "virtual-svc") ;
    dep.nprovide = 1 ;
    dep.conflict = resolve_add_string(w, "other-svc") ;
    dep.nconflict = 1 ;

    if (!resolve_write_cdb(w, dir, "svc.dependencies")) {
        printf("FAIL: resolve_write_cdb\n") ;
        assert(0) ;
    }

    resolve_service_addon_dependencies_t back = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t *wback = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &back) ;
    int r = resolve_read_cdb(wback, dir, "svc.dependencies") ;
    if (r <= 0) {
        printf("FAIL: resolve_read_cdb returned %d\n", r) ;
        assert(0) ;
    }

    assert(!strcmp(back.sa.s + back.depends, "dep1 dep2")) ;
    assert(back.ndepends == 2) ;
    assert(!strcmp(back.sa.s + back.requiredby, "svc-log")) ;
    assert(back.nrequiredby == 1) ;
    assert(!strcmp(back.sa.s + back.optsdeps, "opt1")) ;
    assert(back.noptsdeps == 1) ;
    assert(!strcmp(back.sa.s + back.contents, "a b c")) ;
    assert(back.ncontents == 3) ;
    assert(!strcmp(back.sa.s + back.provide, "virtual-svc")) ;
    assert(back.nprovide == 1) ;
    assert(!strcmp(back.sa.s + back.conflict, "other-svc")) ;
    assert(back.nconflict == 1) ;

    resolve_free(wback) ;
    resolve_free(w) ;

    printf(" PASS\n") ;
}

int main(void)
{
    printf("Starting dependencies addon round-trip test...\n") ;

    char dir[] = "/tmp/66-deps-test.XXXXXX" ;
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

    char file[dlen + 1 + 17] ;
    memcpy(file, dir, dlen) ;
    memcpy(file + dlen, "/svc.dependencies", 18) ;
    unlink(file) ;
    rmdir(dir) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
