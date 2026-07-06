/*
 * regex_roundtrip.c
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

/* Round-trip the autonomous `regex` addon CDB: build the module substitution
 * fields (configure + the three lists and their counts) in memory, write it as
 * its own CDB, read it back and assert every leaf survives. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <66/resolve.h>
#include <66/service.h>

static void test_roundtrip(char const *dir)
{
    printf("Running regex addon round-trip\n") ;

    resolve_service_addon_regex_t rx = RESOLVE_SERVICE_ADDON_REGEX_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_REGEX, &rx) ;
    resolve_init(w) ;

    rx.configure = resolve_add_string(w, "configure.sh") ;
    rx.directories = resolve_add_string(w, "dir1 dir2") ;
    rx.ndirectories = 2 ;
    rx.files = resolve_add_string(w, "file1") ;
    rx.nfiles = 1 ;
    rx.infiles = resolve_add_string(w, ":file:KEY=val") ;
    rx.ninfiles = 1 ;

    if (!resolve_write_cdb(w, dir, "svc.regex")) {
        printf("FAIL: resolve_write_cdb\n") ;
        assert(0) ;
    }

    resolve_service_addon_regex_t back = RESOLVE_SERVICE_ADDON_REGEX_ZERO ;
    resolve_wrapper_t *wback = resolve_set_struct(DATA_SERVICE_REGEX, &back) ;
    int r = resolve_read_cdb(wback, dir, "svc.regex") ;
    if (r <= 0) {
        printf("FAIL: resolve_read_cdb returned %d\n", r) ;
        assert(0) ;
    }

    assert(!strcmp(back.sa.s + back.configure, "configure.sh")) ;
    assert(!strcmp(back.sa.s + back.directories, "dir1 dir2")) ;
    assert(back.ndirectories == 2) ;
    assert(!strcmp(back.sa.s + back.files, "file1")) ;
    assert(back.nfiles == 1) ;
    assert(!strcmp(back.sa.s + back.infiles, ":file:KEY=val")) ;
    assert(back.ninfiles == 1) ;

    resolve_free(wback) ;
    resolve_free(w) ;

    printf(" PASS\n") ;
}

int main(void)
{
    printf("Starting regex addon round-trip test...\n") ;

    char dir[] = "/tmp/66-regex-test.XXXXXX" ;
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
    memcpy(file + dlen, "/svc.regex", 11) ;
    unlink(file) ;
    rmdir(dir) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
