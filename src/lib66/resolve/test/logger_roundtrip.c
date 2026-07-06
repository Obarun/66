/*
 * logger_roundtrip.c
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

/* Round-trip the autonomous `logger` addon CDB: build the config in memory
 * (backup/maxsize/timestamp + the derived run scripts + timeouts), write it as
 * its own CDB, read it back and assert every leaf survives. The logger name and
 * the "want" bit are NOT stored (name is reconstructed, want is the core
 * has_logger flag), so they are not part of the round-trip. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>

static void test_roundtrip(char const *dir)
{
    printf("Running logger addon round-trip\n") ;

    resolve_service_addon_logger_t lg = RESOLVE_SERVICE_ADDON_LOGGER_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_LOGGER, &lg) ;
    resolve_init(w) ;

    lg.backup = 5 ;
    lg.maxsize = 2000000 ;
    lg.timestamp = E_PARSER_TIME_ISO ;
    lg.execute.run.run = resolve_add_string(w, "#!/usr/bin/execlineb -P\n66-execute start svc-log\n") ;
    lg.execute.run.run_user = resolve_add_string(w, "#!/bin/sh\nexec 66-log -- /var/log/66/svc\n") ;
    lg.execute.run.build = resolve_add_string(w, "custom") ;
    lg.execute.run.runas = resolve_add_string(w, "loguser") ;
    lg.execute.timeout.start = 3000 ;
    lg.execute.timeout.stop = 4000 ;

    if (!resolve_write_cdb(w, dir, "svc.logger")) {
        printf("FAIL: resolve_write_cdb\n") ;
        assert(0) ;
    }

    resolve_service_addon_logger_t back = RESOLVE_SERVICE_ADDON_LOGGER_ZERO ;
    resolve_wrapper_t *wback = resolve_set_struct(DATA_SERVICE_LOGGER, &back) ;
    int r = resolve_read_cdb(wback, dir, "svc.logger") ;
    if (r <= 0) {
        printf("FAIL: resolve_read_cdb returned %d\n", r) ;
        assert(0) ;
    }

    assert(back.backup == 5) ;
    assert(back.maxsize == 2000000) ;
    assert(back.timestamp == (uint32_t)E_PARSER_TIME_ISO) ;
    assert(!strcmp(back.sa.s + back.execute.run.run, "#!/usr/bin/execlineb -P\n66-execute start svc-log\n")) ;
    assert(!strcmp(back.sa.s + back.execute.run.run_user, "#!/bin/sh\nexec 66-log -- /var/log/66/svc\n")) ;
    assert(!strcmp(back.sa.s + back.execute.run.build, "custom")) ;
    assert(!strcmp(back.sa.s + back.execute.run.runas, "loguser")) ;
    assert(back.execute.timeout.start == 3000) ;
    assert(back.execute.timeout.stop == 4000) ;

    resolve_free(wback) ;
    resolve_free(w) ;

    printf(" PASS\n") ;
}

int main(void)
{
    printf("Starting logger addon round-trip test...\n") ;

    char dir[] = "/tmp/66-logger-test.XXXXXX" ;
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

    char file[dlen + 1 + 11] ;
    memcpy(file, dir, dlen) ;
    memcpy(file + dlen, "/svc.logger", 12) ;
    unlink(file) ;
    rmdir(dir) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
