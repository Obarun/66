/*
 * parse_store.c -- unit test for the two-pass parse store (pass 1)
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
#include <string.h>

#include <66/parse.h>
#include <66/enum_parser.h>

static char const *g(parse_store_t *st, uint32_t sid, uint32_t kid)
{
    size_t len = 0 ;
    return parse_store_get(st, sid, kid, &len) ;
}

/* A written key reads present with its raw value; an absent one reads absent
 * (present = 0, get = 0) -- the honest "did the user fill this box?" bit. */
static void present_and_value(void)
{
    printf("Running test present_and_value...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "Description = \"a test\"\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "TimeoutStart = 2000\n" ;

    parse_store_t st ;
    assert(parse_store_build(&st, fe) == 1) ;

    /* present + value */
    assert(parse_store_present(&st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_TYPE) == 1) ;
    assert(!strcmp(g(&st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_TYPE), "classic")) ;
    assert(!strcmp(g(&st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_DESCRIPTION), "a test")) ;
    assert(!strcmp(g(&st, E_PARSER_SECTION_START, E_PARSER_SECTION_STARTSTOP_TIMESTART), "2000")) ;

    /* a bracket value is stored raw: the inter-parenthesis text verbatim,
     * leading/trailing spaces included -- trimming/tokenizing is pass 2's job */
    assert(parse_store_present(&st, E_PARSER_SECTION_START, E_PARSER_SECTION_STARTSTOP_EXEC) == 1) ;
    assert(!strcmp(g(&st, E_PARSER_SECTION_START, E_PARSER_SECTION_STARTSTOP_EXEC), " /bin/true ")) ;

    /* absent key -> present 0, get 0 */
    assert(parse_store_present(&st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_VERSION) == 0) ;
    assert(g(&st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_VERSION) == 0) ;

    /* absent section -> its keys read absent */
    assert(parse_store_present(&st, E_PARSER_SECTION_LOGGER, E_PARSER_SECTION_LOGGER_TIMESTAMP) == 0) ;
    assert(parse_store_present(&st, E_PARSER_SECTION_LOGGER, E_PARSER_SECTION_LOGGER_TIMESTART) == 0) ;

    parse_store_free(&st) ;
}

/* THE point of (sid,kid) addressing: TimeoutStart exists in [Start], [Stop] and
 * [Logger] with distinct values. A flat key->value store would collapse them. */
static void same_key_distinct_sections(void)
{
    printf("Running test same_key_distinct_sections...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "TimeoutStart = 2000\n"
        "\n"
        "[Stop]\n"
        "Execute = ( /bin/stop )\n"
        "\n"
        "[Logger]\n"
        "TimeoutStart = 5000\n"
        "Timestamp = iso\n" ;

    parse_store_t st ;
    assert(parse_store_build(&st, fe) == 1) ;

    /* same key name, three sections, three verdicts/values */
    assert(!strcmp(g(&st, E_PARSER_SECTION_START, E_PARSER_SECTION_STARTSTOP_TIMESTART), "2000")) ;
    assert(parse_store_present(&st, E_PARSER_SECTION_STOP, E_PARSER_SECTION_STARTSTOP_TIMESTART) == 0) ;
    assert(!strcmp(g(&st, E_PARSER_SECTION_LOGGER, E_PARSER_SECTION_LOGGER_TIMESTART), "5000")) ;

    assert(!strcmp(g(&st, E_PARSER_SECTION_LOGGER, E_PARSER_SECTION_LOGGER_TIMESTAMP), "iso")) ;

    /* Execute (bracket) is also distinct per section, by value */
    assert(!strcmp(g(&st, E_PARSER_SECTION_START, E_PARSER_SECTION_STARTSTOP_EXEC), " /bin/true ")) ;
    assert(!strcmp(g(&st, E_PARSER_SECTION_STOP, E_PARSER_SECTION_STARTSTOP_EXEC), " /bin/stop ")) ;

    parse_store_free(&st) ;
}

/* Limit* keys live in [Execute]; the store carries them like any other key. */
static void execute_and_environ(void)
{
    printf("Running test execute_and_environ...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "\n"
        "[Execute]\n"
        "LimitNOFILE = 1234\n"
        "\n"
        "[Environment]\n"
        "FOO=bar\n"
        "BAZ=qux\n" ;

    parse_store_t st ;
    assert(parse_store_build(&st, fe) == 1) ;

    assert(!strcmp(g(&st, E_PARSER_SECTION_EXECUTE, E_PARSER_SECTION_EXECUTE_LIMITNOFILE), "1234")) ;

    /* [Environment] is captured as one raw Enval blob */
    assert(parse_store_present(&st, E_PARSER_SECTION_ENVIRONMENT, E_PARSER_SECTION_ENVIRON_ENVAL) == 1) ;
    assert(strstr(g(&st, E_PARSER_SECTION_ENVIRONMENT, E_PARSER_SECTION_ENVIRON_ENVAL), "FOO=bar") != 0) ;

    parse_store_free(&st) ;
}

/* A key written with an empty value is present with a zero-length value -- the
 * whole reason present is a bit separate from the value, and the reason the arena
 * is not an sbl (sbl forbids empty elements). */
static void present_but_empty(void)
{
    printf("Running test present_but_empty...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "Description = \"\"\n"
        "StdOut = \n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n" ;

    parse_store_t st ;
    assert(parse_store_build(&st, fe) == 1) ;

    size_t len = 99 ;
    char const *p = parse_store_get(&st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_DESCRIPTION, &len) ;
    assert(parse_store_present(&st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_DESCRIPTION) == 1) ;
    assert(p != 0 && len == 0 && p[0] == 0) ;

    len = 99 ;
    p = parse_store_get(&st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_STDOUT, &len) ;
    assert(parse_store_present(&st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_STDOUT) == 1) ;
    assert(p != 0 && len == 0 && p[0] == 0) ;

    parse_store_free(&st) ;
}

int main(void)
{
    present_and_value() ;
    same_key_distinct_sections() ;
    execute_and_environ() ;
    present_but_empty() ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
