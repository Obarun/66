/*
 * limit_roundtrip.c
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

/* Round-trip the autonomous `limit` addon CDB: build the addon in memory, write
 * it as its own CDB, read it back, and assert every field survives. Proves the
 * split addon serialises and deserialises on its own. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <66/resolve.h>
#include <66/service.h>

#define ASSERT_EQ(actual, expected, msg) do { \
    unsigned long long _a = (unsigned long long)(actual), _e = (unsigned long long)(expected) ; \
    if (_a != _e) { \
        printf("FAIL: %s (got %llu, expected %llu)\n", msg, _a, _e) ; \
        assert(0) ; \
    } \
} while (0)

static void test_roundtrip(char const *dir)
{
    printf("Running limit addon round-trip\n") ;

    resolve_service_addon_limit_t l = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
    resolve_wrapper_t *wl = resolve_set_struct(DATA_SERVICE_LIMIT, &l) ;
    resolve_init(wl) ;

    l.limitas = 1000000 ;
    l.limitnofile = 2048 ;
    l.limitstack = 8388608 ;
    l.limitnice = 30 ;      /* nice -10 stored as 20-(-10) */
    l.limitrtprio = 100 ;
    /* limitcpu, limitdata, ... left at 0 (unset) */

    if (!resolve_write_cdb(wl, dir, "svc.limit")) {
        printf("FAIL: resolve_write_cdb\n") ;
        assert(0) ;
    }

    resolve_service_addon_limit_t back = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
    resolve_wrapper_t *wback = resolve_set_struct(DATA_SERVICE_LIMIT, &back) ;
    int r = resolve_read_cdb(wback, dir, "svc.limit") ;
    if (r <= 0) {
        printf("FAIL: resolve_read_cdb returned %d\n", r) ;
        assert(0) ;
    }

    ASSERT_EQ(back.limitas, 1000000, "limitas") ;
    ASSERT_EQ(back.limitnofile, 2048, "limitnofile") ;
    ASSERT_EQ(back.limitstack, 8388608, "limitstack") ;
    ASSERT_EQ(back.limitnice, 30, "limitnice") ;
    ASSERT_EQ(back.limitrtprio, 100, "limitrtprio") ;
    /* unset fields survive as 0 */
    ASSERT_EQ(back.limitcpu, 0, "limitcpu") ;
    ASSERT_EQ(back.limitdata, 0, "limitdata") ;
    ASSERT_EQ(back.limitsigpending, 0, "limitsigpending") ;

    resolve_free(wback) ;
    resolve_free(wl) ;

    printf(" PASS\n") ;
}

int main(void)
{
    printf("Starting limit addon round-trip test...\n") ;

    char dir[] = "/tmp/66-limit-test.XXXXXX" ;
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
    memcpy(file + dlen, "/svc.limit", 11) ;
    unlink(file) ;
    rmdir(dir) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
