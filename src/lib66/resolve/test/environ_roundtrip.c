/*
 * environ_roundtrip.c
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

/* Round-trip the autonomous `environ` addon CDB: build the addon in memory (mixed
 * string + integer fields), write it as its own CDB, read it back, and assert
 * every field survives -- strings through the arena, integers verbatim. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <66/resolve.h>
#include <66/service.h>

static void test_roundtrip(char const *dir)
{
    printf("Running environ addon round-trip\n") ;

    resolve_service_addon_environ_t e = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
    resolve_wrapper_t *we = resolve_set_struct(DATA_SERVICE_ENVIRON, &e) ;
    resolve_init(we) ;

    e.env = resolve_add_string(we, "FOO=bar\nBAZ=qux") ;
    e.envdir = resolve_add_string(we, "/etc/66/conf/svc") ;
    e.importfile = resolve_add_string(we, "/etc/svc.env") ;
    e.env_overwrite = 1 ;
    e.nimportfile = 1 ;

    if (!resolve_write_cdb(we, dir, "svc.environ")) {
        printf("FAIL: resolve_write_cdb\n") ;
        assert(0) ;
    }

    resolve_service_addon_environ_t back = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
    resolve_wrapper_t *wback = resolve_set_struct(DATA_SERVICE_ENVIRON, &back) ;
    int r = resolve_read_cdb(wback, dir, "svc.environ") ;
    if (r <= 0) {
        printf("FAIL: resolve_read_cdb returned %d\n", r) ;
        assert(0) ;
    }

    assert(!strcmp(back.sa.s + back.env, "FOO=bar\nBAZ=qux")) ;
    assert(!strcmp(back.sa.s + back.envdir, "/etc/66/conf/svc")) ;
    assert(!strcmp(back.sa.s + back.importfile, "/etc/svc.env")) ;
    assert(back.env_overwrite == 1) ;
    assert(back.nimportfile == 1) ;

    resolve_free(wback) ;
    resolve_free(we) ;

    printf(" PASS\n") ;
}

int main(void)
{
    printf("Starting environ addon round-trip test...\n") ;

    char dir[] = "/tmp/66-environ-test.XXXXXX" ;
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

    char file[dlen + 1 + 12] ;
    memcpy(file, dir, dlen) ;
    memcpy(file + dlen, "/svc.environ", 13) ;
    unlink(file) ;
    rmdir(dir) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
