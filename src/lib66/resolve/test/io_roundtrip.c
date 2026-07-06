/*
 * io_roundtrip.c
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

/* Round-trip the autonomous `io` addon CDB: build the fd triplet in memory
 * (integer types + string destinations), write it as its own CDB, read it back
 * and assert every leaf survives. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>

static void test_roundtrip(char const *dir)
{
    printf("Running io addon round-trip\n") ;

    resolve_service_addon_io_t io = RESOLVE_SERVICE_ADDON_IO_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_IO, &io) ;
    resolve_init(w) ;

    io.fdin.type = E_PARSER_IO_TYPE_66LOG ;
    io.fdin.destination = resolve_add_string(w, "/run/66/scandir/fdholder") ;
    io.fdout.type = E_PARSER_IO_TYPE_66LOG ;
    io.fdout.destination = resolve_add_string(w, "/var/log/66/svc") ;
    io.fderr.type = E_PARSER_IO_TYPE_INHERIT ;
    io.fderr.destination = resolve_add_string(w, "/var/log/66/svc") ;

    if (!resolve_write_cdb(w, dir, "svc.io")) {
        printf("FAIL: resolve_write_cdb\n") ;
        assert(0) ;
    }

    resolve_service_addon_io_t back = RESOLVE_SERVICE_ADDON_IO_ZERO ;
    resolve_wrapper_t *wback = resolve_set_struct(DATA_SERVICE_IO, &back) ;
    int r = resolve_read_cdb(wback, dir, "svc.io") ;
    if (r <= 0) {
        printf("FAIL: resolve_read_cdb returned %d\n", r) ;
        assert(0) ;
    }

    assert(back.fdin.type == E_PARSER_IO_TYPE_66LOG) ;
    assert(!strcmp(back.sa.s + back.fdin.destination, "/run/66/scandir/fdholder")) ;
    assert(back.fdout.type == E_PARSER_IO_TYPE_66LOG) ;
    assert(!strcmp(back.sa.s + back.fdout.destination, "/var/log/66/svc")) ;
    assert(back.fderr.type == E_PARSER_IO_TYPE_INHERIT) ;
    assert(!strcmp(back.sa.s + back.fderr.destination, "/var/log/66/svc")) ;

    resolve_free(wback) ;
    resolve_free(w) ;

    printf(" PASS\n") ;
}

int main(void)
{
    printf("Starting io addon round-trip test...\n") ;

    char dir[] = "/tmp/66-io-test.XXXXXX" ;
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

    char file[dlen + 1 + 7] ;
    memcpy(file, dir, dlen) ;
    memcpy(file + dlen, "/svc.io", 8) ;
    unlink(file) ;
    rmdir(dir) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
