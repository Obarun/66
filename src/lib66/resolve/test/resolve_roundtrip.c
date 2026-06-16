/*
 * resolve_roundtrip.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

/* Round-trip the service resolve CDB: build a resolve_service_t in memory,
 * write it as a CDB, read it back, and assert every field survives. Run under
 * the sanitizers it also exercises the (de)serialisation for memory bugs. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>

#define ASSERT_STR_EQ(actual, expected, msg) do { \
    const char *_a = (actual), *_e = (expected) ; \
    if (strcmp(_a, _e) != 0) { \
        printf("FAIL: %s (got '%s', expected '%s')\n", msg, _a, _e) ; \
        assert(0) ; \
    } \
} while (0)

#define ASSERT_EQ(actual, expected, msg) do { \
    long _a = (long)(actual), _e = (long)(expected) ; \
    if (_a != _e) { \
        printf("FAIL: %s (got %ld, expected %ld)\n", msg, _a, _e) ; \
        assert(0) ; \
    } \
} while (0)

static void test_roundtrip(char const *dir)
{
    printf("Running test_roundtrip\n") ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t *wres = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_init(wres) ;

    res.name = resolve_add_string(wres, "myservice") ;
    res.description = resolve_add_string(wres, "a test service") ;
    res.version = resolve_add_string(wres, "1.2.3") ;
    res.user = resolve_add_string(wres, "root") ;
    res.treename = resolve_add_string(wres, "global") ;
    res.ownerstr = resolve_add_string(wres, "0") ;
    res.dependencies.depends = resolve_add_string(wres, "dep1 dep2") ;
    res.dependencies.ndepends = 2 ;
    res.type = E_PARSER_TYPE_CLASSIC ;
    res.maxdeath = 7 ;
    res.enabled = 1 ;
    res.notify = 3 ;
    res.execute.timeout.start = 5000 ;

    if (!resolve_write_cdb(wres, dir, "svc")) {
        printf("FAIL: resolve_write_cdb\n") ;
        assert(0) ;
    }

    resolve_service_t back = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t *wback = resolve_set_struct(DATA_SERVICE, &back) ;
    int r = resolve_read_cdb(wback, dir, "svc") ;
    if (r <= 0) {
        printf("FAIL: resolve_read_cdb returned %d\n", r) ;
        assert(0) ;
    }

    ASSERT_STR_EQ(back.sa.s + back.name, "myservice", "name") ;
    ASSERT_STR_EQ(back.sa.s + back.description, "a test service", "description") ;
    ASSERT_STR_EQ(back.sa.s + back.version, "1.2.3", "version") ;
    ASSERT_STR_EQ(back.sa.s + back.user, "root", "user") ;
    ASSERT_STR_EQ(back.sa.s + back.treename, "global", "treename") ;
    ASSERT_STR_EQ(back.sa.s + back.dependencies.depends, "dep1 dep2", "depends") ;
    ASSERT_EQ(back.dependencies.ndepends, 2, "ndepends") ;
    ASSERT_EQ(back.type, E_PARSER_TYPE_CLASSIC, "type") ;
    ASSERT_EQ(back.maxdeath, 7, "maxdeath") ;
    ASSERT_EQ(back.enabled, 1, "enabled") ;
    ASSERT_EQ(back.notify, 3, "notify") ;
    ASSERT_EQ(back.execute.timeout.start, 5000, "timeout.start") ;

    resolve_free(wback) ;
    resolve_free(wres) ;

    printf(" PASS\n") ;
}

int main(void)
{
    printf("Starting resolve round-trip tests...\n") ;

    char dir[] = "/tmp/66-resolve-test.XXXXXX" ;
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

    /* cleanup */
    char file[dlen + 1 + 4] ;
    memcpy(file, dir, dlen) ;
    memcpy(file + dlen, "/svc", 5) ;
    unlink(file) ;
    rmdir(dir) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
