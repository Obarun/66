/*
 * parse_environ.c -- unit test for the store-based environ addon parser
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

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> // mkstemp
#include <string.h>
#include <unistd.h> // close, unlink

#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/parse.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/enum_parser.h>

/* [Environment] -> environ addon: the raw env block, the computed envdir (from
 * the core name/owner), the ImportFile split, the count, and env_overwrite (the
 * out-of-band conf flag). */
static void with_environment(void)
{
    printf("Running test with_environment...\n") ;

    /* ImportFile is validated as an existing non-directory absolute path */
    char imp[] = "/tmp/66-envimp.XXXXXX" ;
    int fd = mkstemp(imp) ;
    assert(fd >= 0) ;
    close(fd) ;

    _cleanup_strbuf_ strbuf fe = STRBUF_ZERO ;
    assert(auto_strbuf(&fe,
        "[Main]\n"
        "Type = classic\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "\n"
        "[Environment]\n"
        "FOO=bar\n"
        "BAZ=qux\n"
        "ImportFile=", imp, "\n")) ;

    parse_store_t st ;
    assert(parse_store_build(&st, fe.s) == 1) ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_init(w) ;
    res.name = resolve_add_string(w, "testsvc") ;
    res.owner = 0 ; // admin -> envdir under SS_SERVICE_ADMCONFDIR

    resolve_service_addon_environ_t e = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
    uint8_t has_environ = 0 ;
    assert(parse_environ(&st, &res, &e, 1, &has_environ) == 1) ;

    assert(has_environ == 1) ;
    assert(e.env_overwrite == 1) ;                        // the conf flag

    /* the env block, ImportFile stripped out */
    assert(strstr(e.sa.s + e.env, "FOO=bar") != 0) ;
    assert(strstr(e.sa.s + e.env, "BAZ=qux") != 0) ;
    assert(strstr(e.sa.s + e.env, "ImportFile") == 0) ;

    /* envdir computed from the core: SS_SERVICE_ADMCONFDIR + name */
    char expected[256] ;
    auto_strings(expected, SS_SERVICE_ADMCONFDIR, "testsvc") ;
    assert(!strcmp(e.sa.s + e.envdir, expected)) ;

    /* the import file, split out and counted */
    assert(e.nimportfile == 1) ;
    assert(!strcmp(e.sa.s + e.importfile, imp)) ;

    strbuf_free(&e.sa) ;
    resolve_free(w) ;
    parse_store_free(&st) ;
    unlink(imp) ;
}

/* No [Environment] section -> no addon. */
static void without_environment(void)
{
    printf("Running test without_environment...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n" ;

    parse_store_t st ;
    assert(parse_store_build(&st, fe) == 1) ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_init(w) ;
    res.name = resolve_add_string(w, "testsvc") ;

    resolve_service_addon_environ_t e = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
    uint8_t has_environ = 1 ; // seed non-zero to prove parse_environ clears it
    assert(parse_environ(&st, &res, &e, 0, &has_environ) == 1) ;

    assert(has_environ == 0) ;
    assert(e.env == 0) ;
    assert(e.envdir == 0) ;

    strbuf_free(&e.sa) ;
    resolve_free(w) ;
    parse_store_free(&st) ;
}

int main(void)
{
    with_environment() ;
    without_environment() ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
