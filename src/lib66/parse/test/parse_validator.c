/*
 * parse_validator.c -- unit test for the parser state validator
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#include <66/parse.h>
#include <66/enum_parser.h>

static resolve_enum_table_t key_main(uint32_t id)
{
    resolve_enum_table_t t = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
    t.u.parser.id = id ;
    return t ;
}

static resolve_enum_table_t key_start(uint32_t id)
{
    resolve_enum_table_t t = E_TABLE_PARSER_SECTION_START_ZERO ;
    t.u.parser.id = id ;
    return t ;
}

static resolve_enum_table_t key_stop(uint32_t id)
{
    resolve_enum_table_t t = E_TABLE_PARSER_SECTION_STOP_ZERO ;
    t.u.parser.id = id ;
    return t ;
}

static resolve_enum_table_t key_logger(uint32_t id)
{
    resolve_enum_table_t t = E_TABLE_PARSER_SECTION_LOGGER_ZERO ;
    t.u.parser.id = id ;
    return t ;
}

/* A key present in the frontend reads as touched, an absent one as default,
 * each looked up only in the section it belongs to. */
static void present_and_absent(void)
{
    printf("Running test present_and_absent...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "Description = \"test\"\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "TimeoutStart = 2000\n"
        "\n"
        "[Stop]\n"
        "Execute = ( /bin/stop )\n"
        "\n"
        "[Logger]\n"
        "Backup = 3\n"
        "Timestamp = iso\n" ;

    parse_validator_t v ;
    assert(parse_validator_init(&v, fe) == 1) ;

    /* [Main] */
    assert(parse_checker(&v, key_main(E_PARSER_SECTION_MAIN_TYPE)) == 1) ;
    assert(parse_checker(&v, key_main(E_PARSER_SECTION_MAIN_DESCRIPTION)) == 1) ;
    assert(parse_checker(&v, key_main(E_PARSER_SECTION_MAIN_VERSION)) == 0) ;

    /* [Start] */
    assert(parse_checker(&v, key_start(E_PARSER_SECTION_STARTSTOP_EXEC)) == 1) ;
    assert(parse_checker(&v, key_start(E_PARSER_SECTION_STARTSTOP_TIMESTART)) == 1) ;

    /* [Stop]: Execute set, TimeoutStop not */
    assert(parse_checker(&v, key_stop(E_PARSER_SECTION_STARTSTOP_EXEC)) == 1) ;
    assert(parse_checker(&v, key_stop(E_PARSER_SECTION_STARTSTOP_TIMESTOP)) == 0) ;

    /* [Logger] */
    assert(parse_checker(&v, key_logger(E_PARSER_SECTION_LOGGER_TIMESTAMP)) == 1) ;
    assert(parse_checker(&v, key_logger(E_PARSER_SECTION_LOGGER_BACKUP)) == 1) ;
    assert(parse_checker(&v, key_logger(E_PARSER_SECTION_LOGGER_MAXSIZE)) == 0) ;
}

/* The whole point of the section scope: TimeoutStart lives in [Start]. A
 * whole-file scan would wrongly report it present when asked for [Stop]. */
static void section_scoping(void)
{
    printf("Running test section_scoping...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "TimeoutStart = 2000\n"
        "\n"
        "[Stop]\n"
        "Execute = ( /bin/stop )\n" ;

    parse_validator_t v ;
    assert(parse_validator_init(&v, fe) == 1) ;

    assert(parse_checker(&v, key_start(E_PARSER_SECTION_STARTSTOP_TIMESTART)) == 1) ;
    assert(parse_checker(&v, key_stop(E_PARSER_SECTION_STARTSTOP_TIMESTART)) == 0) ;
}

/* A key of an entirely absent section reads as default. */
static void missing_section(void)
{
    printf("Running test missing_section...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n" ;

    parse_validator_t v ;
    assert(parse_validator_init(&v, fe) == 1) ;

    assert(parse_checker(&v, key_logger(E_PARSER_SECTION_LOGGER_TIMESTAMP)) == 0) ;
    assert(parse_checker(&v, key_stop(E_PARSER_SECTION_STARTSTOP_EXEC)) == 0) ;
}

int main(void)
{
    present_and_absent() ;
    section_scoping() ;
    missing_section() ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
