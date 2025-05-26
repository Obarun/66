/*
 * enum.h
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_ENUM_H
#define SS_ENUM_H

#include <sys/types.h>
#include <66/enum_struct.h>
#include <66/enum_parser.h>
#include <66/enum_service.h>
#include <66/enum_tree.h>

#define CATEGORY_RESOLVE(id) E_RESOLVE_CATEGORY_##id

#define RESOLVE_CATEGORY(macro) \
    macro(PARSER), \
    macro(SERVICE), \
    macro(TREE)

enum resolve_enum_category_e
{
    RESOLVE_CATEGORY(CATEGORY_RESOLVE),
    E_RESOLVE_CATEGORY_ENDOFKEY
} ;
typedef enum resolve_enum_category_e resolve_enum_category_t ;

typedef struct resolve_enum_table_s resolve_enum_table_t ;
struct resolve_enum_table_s
{
    resolve_enum_category_t category ;
    union {
        resolve_parser_enum_table_t parser ;
        resolve_service_enum_table_t service ;
        resolve_tree_enum_table_t tree ;
    } u ;
} ;

extern char const *enum_to_key(key_description_t const *list, int const key) ;
extern ssize_t key_to_enum(key_description_t const *list, char const *key) ;
extern key_description_t const *enum_get_list(resolve_enum_table_t table) ;
extern int get_endofkey(const key_description_t *list) ;

#endif