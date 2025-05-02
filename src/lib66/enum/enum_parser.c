/*
 * enum_parser.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>
#include <errno.h>
#include <66/enum_struct.h>
#include <66/enum_parser.h>

const char *enum_str_parser_expected[] = {
    EXPECTED_TEMPLATE(STR_EXPECTED),
    0
} ;

key_description_t const enum_list_parser_expected[] = {
    EXPECTED_TEMPLATE(KEY_EXPECTED),
    { .name = 0 }
} ;

const char *enum_str_parser_section[] = {
    SECTION_TEMPLATE(STR_SECTION),
    0
} ;

key_description_t const enum_list_parser_section[] = {
    SECTION_TEMPLATE(KEY_SECTION),
    { .name = 0 }
} ;

const char *enum_str_parser_section_main[] = {
    SECTION_MAIN_TEMPLATE(STR_SECTION_MAIN),
    0
} ;

key_description_t const enum_list_parser_section_main[] = {
    SECTION_MAIN_TEMPLATE(KEY_SECTION_MAIN),
    { .name = 0 }
} ;

const char *enum_str_parser_section_startstop[] = {
    SECTION_STARTSTOP_TEMPLATE(STR_SECTION_STARTSTOP),
    0
} ;

key_description_t const enum_list_parser_section_startstop[] = {
    SECTION_STARTSTOP_TEMPLATE(KEY_SECTION_STARTSTOP),
    { .name = 0 }
} ;

const char *enum_str_parser_section_logger[] = {
    SECTION_LOGGER_TEMPLATE(STR_SECTION_LOGGER),
    0
} ;

key_description_t const enum_list_parser_section_logger[] = {
    SECTION_LOGGER_TEMPLATE(KEY_SECTION_LOGGER),
    { .name = 0 }
} ;

const char *enum_str_parser_section_environ[] = {
    SECTION_ENVIRON_TEMPLATE(STR_SECTION_ENVIRON),
    0
} ;

key_description_t const enum_list_parser_section_environ[] = {
    SECTION_ENVIRON_TEMPLATE(KEY_SECTION_ENVIRON),
    { .name = 0 }
} ;

const char *enum_str_parser_section_regex[] = {
    SECTION_REGEX_TEMPLATE(STR_SECTION_REGEX),
    0
} ;

key_description_t const enum_list_parser_section_regex[] = {
    SECTION_REGEX_TEMPLATE(KEY_SECTION_REGEX),
    { .name = 0 }
} ;

const char *enum_str_parser_section_execute[] = {
    SECTION_EXECUTE_TEMPLATE(STR_SECTION_EXECUTE),
    0
} ;

key_description_t const enum_list_parser_section_execute[] = {
    SECTION_EXECUTE_TEMPLATE(KEY_SECTION_EXECUTE),
    { .name = 0 }
} ;

const char *enum_str_parser_io_type[] = {
    IO_TYPE_TEMPLATE(STR_IO_TYPE),
    0
} ;

key_description_t const enum_list_parser_io_type[] = {
    IO_TYPE_TEMPLATE(KEY_IO_TYPE),
    { .name = 0 }
} ;

const char *enum_str_parser_type[] = {
    TYPE_TEMPLATE(STR_TYPE),
    0
} ;

key_description_t const enum_list_parser_type[] = {
    TYPE_TEMPLATE(KEY_TYPE),
    { .name = 0 }
} ;

const char *enum_str_parser_opts[] = {
    OPTS_TEMPLATE(STR_OPTS),
    0
} ;

key_description_t const enum_list_parser_opts[] = {
    OPTS_TEMPLATE(KEY_OPTS),
    { .name = 0 }
} ;

const char *enum_str_parser_flags[] = {
    FLAGS_TEMPLATE(STR_FLAGS),
    0
} ;

key_description_t const enum_list_parser_flags[] = {
    FLAGS_TEMPLATE(KEY_FLAGS),
    { .name = 0 }
} ;

const char *enum_str_parser_build[] = {
    BUILD_TEMPLATE(STR_BUILD),
    0
} ;

key_description_t const enum_list_parser_build[] = {
    BUILD_TEMPLATE(KEY_BUILD),
    { .name = 0 }
} ;

const char *enum_str_parser_time[] = {
    TIME_TEMPLATE(STR_TIME),
    0
} ;

key_description_t const enum_list_parser_time[] = {
    TIME_TEMPLATE(KEY_TIME),
    { .name = 0 }
} ;

const char *enum_str_parser_seed[] = {
    SEED_TEMPLATE(STR_SEED),
    0
} ;

key_description_t const enum_list_parser_seed[] = {
    SEED_TEMPLATE(KEY_SEED),
    { .name = 0 }
} ;

const char *enum_str_parser_mandatory[] = {
    MANDATORY_TEMPLATE(STR_MANDATORY),
    0
} ;

key_description_t const enum_list_parser_mandatory[] = {
    MANDATORY_TEMPLATE(KEY_MANDATORY),
    { .name = 0 }
} ;

key_description_t const *enum_get_list_parser(resolve_parser_enum_table_t table)
{
    switch (table.category) {

        case E_PARSER_CATEGORY_SECTION_MAIN:
            return enum_list_parser_section_main ;

        case E_PARSER_CATEGORY_SECTION_STARTSTOP:
            return enum_list_parser_section_startstop ;

        case E_PARSER_CATEGORY_SECTION_LOGGER:
            return enum_list_parser_section_logger ;

        case E_PARSER_CATEGORY_SECTION_ENVIRON:
            return enum_list_parser_section_environ ;

        case E_PARSER_CATEGORY_SECTION_REGEX:
            return enum_list_parser_section_regex ;

        case E_PARSER_CATEGORY_SECTION:
            return enum_list_parser_section ;

        case E_PARSER_CATEGORY_IO_TYPE:
            return enum_list_parser_io_type ;

        case E_PARSER_CATEGORY_TYPE:
            return enum_list_parser_type ;

        case E_PARSER_CATEGORY_OPTS:
            return enum_list_parser_opts ;

        case E_PARSER_CATEGORY_FLAGS:
            return enum_list_parser_flags ;

        case E_PARSER_CATEGORY_BUILD:
            return enum_list_parser_build ;

        case E_PARSER_CATEGORY_TIME:
            return enum_list_parser_time ;

        case E_PARSER_CATEGORY_SEED:
            return enum_list_parser_seed ;

        case E_PARSER_CATEGORY_EXPECTED:
            return enum_list_parser_expected ;

        case E_PARSER_CATEGORY_MANDATORY:
            return enum_list_parser_mandatory ;

        default:
            errno = EINVAL ;
            return NULL ;
    }
}

