/*
 * execute_roundtrip.c
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

/* Round-trip the autonomous `execute` addon CDB: build the run/finish scripts,
 * caps, the supervision scalars (notify/maxdeath/maxdeathtime) and the misc
 * integers in memory, write it as its own CDB, read it back and assert every
 * leaf survives. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>

static void test_roundtrip(char const *dir)
{
    printf("Running execute addon round-trip\n") ;

    resolve_service_addon_execute_t ex = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_EXECUTE, &ex) ;
    resolve_init(w) ;

    ex.notify = 3 ;
    ex.maxdeath = 7 ;
    ex.maxdeathtime = 12345 ;
    ex.run.run = resolve_add_string(w, "#!/usr/bin/execlineb -P\nforeground { echo run }\n") ;
    ex.run.run_user = resolve_add_string(w, "#!/bin/sh\nexec /bin/true\n") ;
    ex.run.build = resolve_add_string(w, "custom") ;
    ex.run.runas = resolve_add_string(w, "root") ;
    ex.finish.run = resolve_add_string(w, "#!/usr/bin/execlineb -P\nforeground { echo finish }\n") ;
    ex.timeout.start = 5000 ;
    ex.timeout.stop = 6000 ;
    ex.down = 1 ;
    ex.downsignal = 15 ;
    ex.blockprivileges = 1 ;
    ex.umask = 0022 ;
    ex.want_umask = 1 ;
    ex.nice = 25 ;
    ex.want_nice = 1 ;
    ex.chdir = resolve_add_string(w, "/var/lib/svc") ;
    ex.capsbound = resolve_add_string(w, "cap_net_bind_service") ;
    ex.ncapsbound = 1 ;

    if (!resolve_write_cdb(w, dir, "svc.execute")) {
        printf("FAIL: resolve_write_cdb\n") ;
        assert(0) ;
    }

    resolve_service_addon_execute_t back = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_wrapper_t *wback = resolve_set_struct(DATA_SERVICE_EXECUTE, &back) ;
    int r = resolve_read_cdb(wback, dir, "svc.execute") ;
    if (r <= 0) {
        printf("FAIL: resolve_read_cdb returned %d\n", r) ;
        assert(0) ;
    }

    assert(back.notify == 3) ;
    assert(back.maxdeath == 7) ;
    assert(back.maxdeathtime == 12345) ;
    assert(!strcmp(back.sa.s + back.run.run, "#!/usr/bin/execlineb -P\nforeground { echo run }\n")) ;
    assert(!strcmp(back.sa.s + back.run.run_user, "#!/bin/sh\nexec /bin/true\n")) ;
    assert(!strcmp(back.sa.s + back.run.build, "custom")) ;
    assert(!strcmp(back.sa.s + back.run.runas, "root")) ;
    assert(!strcmp(back.sa.s + back.finish.run, "#!/usr/bin/execlineb -P\nforeground { echo finish }\n")) ;
    assert(back.timeout.start == 5000) ;
    assert(back.timeout.stop == 6000) ;
    assert(back.down == 1) ;
    assert(back.downsignal == 15) ;
    assert(back.blockprivileges == 1) ;
    assert(back.umask == 0022) ;
    assert(back.want_umask == 1) ;
    assert(back.nice == 25) ;
    assert(back.want_nice == 1) ;
    assert(!strcmp(back.sa.s + back.chdir, "/var/lib/svc")) ;
    assert(!strcmp(back.sa.s + back.capsbound, "cap_net_bind_service")) ;
    assert(back.ncapsbound == 1) ;

    resolve_free(wback) ;
    resolve_free(w) ;

    printf(" PASS\n") ;
}

int main(void)
{
    printf("Starting execute addon round-trip test...\n") ;

    char dir[] = "/tmp/66-execute-test.XXXXXX" ;
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
    memcpy(file + dlen, "/svc.execute", 13) ;
    unlink(file) ;
    rmdir(dir) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
