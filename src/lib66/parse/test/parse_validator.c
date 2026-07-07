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
#include <string.h>

#include <66/parse.h>
#include <66/enum_parser.h>

/* parse_validator_init splits the frontend into sections: present[sid] flags an
 * existing section, off[sid]/len[sid] delimit its body. */
static void sections_present_and_absent(void)
{
    printf("Running test sections_present_and_absent...\n") ;

    static char const fe[] =
        "[Main]\n"
        "Type = classic\n"
        "Description = \"test\"\n"
        "\n"
        "[Start]\n"
        "Execute = ( /bin/true )\n"
        "\n"
        "[Logger]\n"
        "Backup = 3\n" ;

    parse_validator_t v ;
    assert(parse_validator_init(&v, fe) == 1) ;

    /* present sections */
    assert(v.present[E_PARSER_SECTION_MAIN]) ;
    assert(v.present[E_PARSER_SECTION_START]) ;
    assert(v.present[E_PARSER_SECTION_LOGGER]) ;

    /* absent sections read as not present */
    assert(!v.present[E_PARSER_SECTION_STOP]) ;
    assert(!v.present[E_PARSER_SECTION_ENVIRONMENT]) ;
    assert(!v.present[E_PARSER_SECTION_REGEX]) ;

    /* off/len delimit the [Main] body: it covers both [Main] keys but stops
     * before the next section header. */
    char main_body[v.len[E_PARSER_SECTION_MAIN] + 1] ;
    memcpy(main_body, v.frontend + v.off[E_PARSER_SECTION_MAIN], v.len[E_PARSER_SECTION_MAIN]) ;
    main_body[v.len[E_PARSER_SECTION_MAIN]] = 0 ;
    assert(strstr(main_body, "Type = classic")) ;
    assert(strstr(main_body, "Description")) ;
    assert(!strstr(main_body, "[Start]")) ;
}

int main(void)
{
    sections_present_and_absent() ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
