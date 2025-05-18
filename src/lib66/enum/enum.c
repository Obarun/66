/*
 * enum.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 * */

#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <66/enum.h>
#include <66/enum_struct.h>
#include <66/enum_parser.h>
#include <66/enum_service.h>
#include <66/enum_tree.h>

#define ENDOFKEY_MAPPING \
    X(enum_list_parser_expected, E_PARSER_EXPECT_ENDOFKEY) \
    X(enum_list_parser_section, E_PARSER_SECTION_ENDOFKEY) \
    X(enum_list_parser_section_main, E_PARSER_SECTION_MAIN_ENDOFKEY) \
    X(enum_list_parser_section_startstop, E_PARSER_SECTION_STARTSTOP_ENDOFKEY) \
    X(enum_list_parser_section_logger, E_PARSER_SECTION_LOGGER_ENDOFKEY) \
    X(enum_list_parser_section_environ, E_PARSER_SECTION_ENVIRON_ENDOFKEY) \
    X(enum_list_parser_section_regex, E_PARSER_SECTION_EXECUTE_ENDOFKEY) \
    X(enum_list_parser_section_execute, E_PARSER_SECTION_EXECUTE_ENDOFKEY) \
    X(enum_list_parser_io_type, E_PARSER_IO_TYPE_ENDOFKEY) \
    X(enum_list_parser_type, E_PARSER_TYPE_ENDOFKEY) \
    X(enum_list_parser_opts, E_PARSER_OPTS_ENDOFKEY) \
    X(enum_list_parser_flags, E_PARSER_FLAGS_ENDOFKEY) \
    X(enum_list_parser_build, E_PARSER_BUILD_ENDOFKEY) \
    X(enum_list_parser_time, E_PARSER_TIME_ENDOFKEY) \
    X(enum_list_parser_seed, E_PARSER_SEED_ENDOFKEY) \
    X(enum_list_parser_mandatory, E_PARSER_MANDATORY_ENDOFKEY) \
    X(enum_list_parser_caps, E_PARSER_CAPS_ENDOFKEY) \
    X(enum_list_service_config, E_RESOLVE_SERVICE_CONFIG_ENDOFKEY) \
    X(enum_list_service_path, E_RESOLVE_SERVICE_PATH_ENDOFKEY) \
    X(enum_list_service_deps, E_RESOLVE_SERVICE_DEPS_ENDOFKEY) \
    X(enum_list_service_execute, E_RESOLVE_SERVICE_EXECUTE_ENDOFKEY) \
    X(enum_list_service_live, E_RESOLVE_SERVICE_LIVE_ENDOFKEY) \
    X(enum_list_service_logger, E_RESOLVE_SERVICE_LOGGER_ENDOFKEY) \
    X(enum_list_service_environ, E_RESOLVE_SERVICE_ENVIRON_ENDOFKEY) \
    X(enum_list_service_regex, E_RESOLVE_SERVICE_REGEX_ENDOFKEY) \
    X(enum_list_service_io, E_RESOLVE_SERVICE_IO_ENDOFKEY) \
    X(enum_list_service_limit, E_RESOLVE_SERVICE_LIMIT_ENDOFKEY) \
    X(enum_list_tree, E_RESOLVE_TREE_ENDOFKEY) \
    X(enum_list_tree_master, E_RESOLVE_TREE_MASTER_ENDOFKEY)

#define X(list, endofkey) if (list_ptr == list) return endofkey ;

int get_endofkey(key_description_t const *list_ptr)
{
    ENDOFKEY_MAPPING
    return -1 ; // Invalid or unrecognized list
}

#undef X
#undef ENDOFKEY_MAPPING

char const *enum_to_key(key_description_t const *list, int const key)
{
    int const endofkey = get_endofkey(list) ;
    if (endofkey < 0 || !list || key < 0 || key >= endofkey) return NULL ;
    return *list[key].name ;
}

ssize_t key_to_enum(key_description_t const *list, char const *key)
{
    if (!list || !key) return -1 ;
    int i = 0 ;
    for(; list[i].name ; i++) {
        if(!strcmp(key, *list[i].name))
            return i ;
    }
    return -1 ;
}

key_description_t const *enum_get_list(resolve_enum_table_t table)
{
    switch (table.category) {
        case E_RESOLVE_CATEGORY_PARSER:
            return enum_get_list_parser(table.u.parser) ;
        case E_RESOLVE_CATEGORY_SERVICE:
            return enum_get_list_service(table.u.service) ;
        case E_RESOLVE_CATEGORY_TREE:
            return enum_get_list_tree(table.u.tree) ;
        default:
            errno = EINVAL ;
            return NULL ;
    }
}
