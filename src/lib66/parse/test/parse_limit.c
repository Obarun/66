/*
 * parse_limit.c -- unit test for the store-based limit addon parser
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
#include <sys/resource.h> // RLIM_INFINITY

#include <66/parse.h>
#include <66/service.h>
#include <66/enum_parser.h>

/* Limits are decoded from the store: plain u64, 'u' -> infinity, RTPRIO clamped
 * to 100, Nice encoded as 20 - n (n == 0 -> 1). has_limit is set as soon as one
 * Limit* key is present. */
static void decode(void)
{
    printf("Running test decode...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "\n"
        "[Execute]\n"
        "LimitNOFILE = 1234\n"
        "LimitAS = u\n"
        "LimitRTPRIO = 150\n"
        "LimitNICE = -5\n" ;

    parse_store_t st ;
    assert(parse_store_build(&st, fe) == 1) ;

    resolve_service_addon_limit_t l = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
    uint8_t has_limit = 0 ;
    assert(parse_limit(&st, &l, &has_limit) == 1) ;

    assert(has_limit == 1) ;
    assert(l.limitnofile == 1234) ;
    assert(l.limitas == (uint64_t)RLIM_INFINITY) ;
    assert(l.limitrtprio == 100) ;      // 150 clamped
    assert(l.limitnice == 25) ;         // 20 - (-5)

    parse_store_free(&st) ;
}

/* No Limit* key -> addon absent, fields left at their default 0. */
static void absent(void)
{
    printf("Running test absent...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n" ;

    parse_store_t st ;
    assert(parse_store_build(&st, fe) == 1) ;

    resolve_service_addon_limit_t l = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
    uint8_t has_limit = 1 ; // seed non-zero to prove parse_limit clears it
    assert(parse_limit(&st, &l, &has_limit) == 1) ;

    assert(has_limit == 0) ;
    assert(l.limitnofile == 0) ;
    assert(l.limitas == 0) ;

    parse_store_free(&st) ;
}

/* Nice == 0 encodes to 1 (not 20), the special-cased boundary. */
static void nice_zero(void)
{
    printf("Running test nice_zero...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "\n"
        "[Execute]\n"
        "LimitNICE = 0\n" ;

    parse_store_t st ;
    assert(parse_store_build(&st, fe) == 1) ;

    resolve_service_addon_limit_t l = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
    uint8_t has_limit = 0 ;
    assert(parse_limit(&st, &l, &has_limit) == 1) ;

    assert(has_limit == 1) ;
    assert(l.limitnice == 1) ;

    parse_store_free(&st) ;
}

int main(void)
{
    decode() ;
    absent() ;
    nice_zero() ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
