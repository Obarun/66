/*
 * enum_service.c
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
#include <66/enum_service.h>

const char *enum_str_service_config[] = {
    CONFIG_TEMPLATE(STR_SERVICE_CONFIG),
    0
} ;

key_description_t const enum_list_service_config[] = {
    CONFIG_TEMPLATE(KEY_SERVICE_CONFIG),
    { .name = 0 }
} ;

const char *enum_str_service_path[] = {
    PATH_TEMPLATE(STR_SERVICE_PATH),
    0
} ;

key_description_t const enum_list_service_path[] = {
    PATH_TEMPLATE(KEY_SERVICE_PATH),
    { .name = 0 }
} ;

const char *enum_str_service_deps[] = {
    DEPS_TEMPLATE(STR_SERVICE_DEPS),
    0
} ;

key_description_t const enum_list_service_deps[] = {
    DEPS_TEMPLATE(KEY_SERVICE_DEPS),
    { .name = 0 }
} ;

const char *enum_str_service_execute[] = {
    EXECUTE_TEMPLATE(STR_SERVICE_EXECUTE),
    0
} ;

key_description_t const enum_list_service_execute[] = {
    EXECUTE_TEMPLATE(KEY_SERVICE_EXECUTE),
    { .name = 0 }
} ;

const char *enum_str_service_live[] = {
    LIVE_TEMPLATE(STR_SERVICE_LIVE),
    0
} ;

key_description_t const enum_list_service_live[] = {
    LIVE_TEMPLATE(KEY_SERVICE_LIVE),
    { .name = 0 }
} ;

const char *enum_str_service_environ[] = {
    ENVIRON_TEMPLATE(STR_SERVICE_ENVIRON),
    0
} ;

key_description_t const enum_list_service_environ[] = {
    ENVIRON_TEMPLATE(KEY_SERVICE_ENVIRON),
    { .name = 0 }
} ;

const char *enum_str_service_regex[] = {
    REGEX_TEMPLATE(STR_SERVICE_REGEX),
    0
} ;

key_description_t const enum_list_service_regex[] = {
    REGEX_TEMPLATE(KEY_SERVICE_REGEX),
    { .name = 0 }
} ;

const char *enum_str_service_io[] = {
    IO_TEMPLATE(STR_SERVICE_IO),
    0
} ;

key_description_t const enum_list_service_io[] = {
    IO_TEMPLATE(KEY_SERVICE_IO),
    { .name = 0 }
} ;

const char *enum_str_service_limit[] = {
    LIMIT_TEMPLATE(STR_SERVICE_LIMIT),
    0
} ;

key_description_t const enum_list_service_limit[] = {
    LIMIT_TEMPLATE(KEY_SERVICE_LIMIT),
    { .name = 0 }
} ;

key_description_t const *enum_get_list_service(resolve_service_enum_table_t table)
{
    switch (table.category) {

        case E_RESOLVE_SERVICE_CATEGORY_CONFIG:
            return enum_list_service_config ;

        case E_RESOLVE_SERVICE_CATEGORY_PATH:
            return enum_list_service_path ;

        case E_RESOLVE_SERVICE_CATEGORY_DEPS:
            return enum_list_service_deps ;

        case E_RESOLVE_SERVICE_CATEGORY_EXECUTE:
            return enum_list_service_execute ;

        case E_RESOLVE_SERVICE_CATEGORY_LIVE:
            return enum_list_service_live ;

        case E_RESOLVE_SERVICE_CATEGORY_ENVIRON:
            return enum_list_service_environ ;

        case E_RESOLVE_SERVICE_CATEGORY_REGEX:
            return enum_list_service_regex ;

        case E_RESOLVE_SERVICE_CATEGORY_IO:
            return enum_list_service_io ;

        case E_RESOLVE_SERVICE_CATEGORY_LIMIT:
            return enum_list_service_limit ;

        default:
            errno = EINVAL ;
            return NULL ;
    }
}
