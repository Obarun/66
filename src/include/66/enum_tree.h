/*
 * enum_tree.h
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

#ifndef SS_ENUM_TREE_H
#define SS_ENUM_TREE_H

#include <stdint.h>
#include <66/enum_struct.h>

#define ENUM_TREE(id, str) E_RESOLVE_TREE_##id
#define ENUM_MASTER(id, str) E_RESOLVE_TREE_MASTER_##id
#define STR_TREE(id, str) [E_RESOLVE_TREE_##id] = str
#define STR_MASTER(id, str) [E_RESOLVE_TREE_MASTER_##id] = str
#define KEY_TREE(idy, str) { .name = &enum_str_tree[E_RESOLVE_TREE_##idy], .id = E_RESOLVE_TREE_##idy }
#define KEY_MASTER(idy, str) { .name = &enum_str_tree_master[E_RESOLVE_TREE_MASTER_##idy], .id = E_RESOLVE_TREE_MASTER_##idy }
#define CATEGORY_TREE(id) E_RESOLVE_TREE_CATEGORY_##id

#define TREE_TEMPLATE(macro) \
    macro(RVERSION, "rversion"), \
    macro(NAME, "name"), \
    macro(ENABLED, "enabled"), \
    macro(DEPENDS, "depends"), \
    macro(REQUIREDBY, "requiredby"), \
    macro(ALLOW, "allow"), \
    macro(GROUPS, "groups"), \
    macro(CONTENTS, "contents"), \
    macro(NDEPENDS, "ndepends"), \
    macro(NREQUIREDBY, "nrequiredby"), \
    macro(NALLOW, "nallow"), \
    macro(NGROUPS, "ngroups"), \
    macro(NCONTENTS, "ncontents"), \
    macro(INIT, "init"), \
    macro(SUPERVISED, "supervised")

#define MASTER_TEMPLATE(macro) \
    macro(RVERSION, "rversion"), \
    macro(NAME, "name"), \
    macro(ALLOW, "allow"), \
    macro(CURRENT, "current"), \
    macro(CONTENTS, "contents"), \
    macro(NALLOW, "nallow"), \
    macro(NCONTENTS, "ncontents")

#define TREE_CATEGORY(macro) \
    macro(TREE), \
    macro(MASTER)

enum resolve_tree_enum_e
{
    TREE_TEMPLATE(ENUM_TREE),
    E_RESOLVE_TREE_ENDOFKEY
} ;
typedef enum resolve_tree_enum_e resolve_tree_enum_t ;

enum resolve_tree_master_enum_e
{
    MASTER_TEMPLATE(ENUM_MASTER),
    E_RESOLVE_TREE_MASTER_ENDOFKEY
} ;
typedef enum resolve_tree_master_enum_e resolve_tree_master_enum_t ;

enum resolve_tree_enum_category_e
{
    TREE_CATEGORY(CATEGORY_TREE),
    E_RESOLVE_TREE_CATEGORY_ENDOFKEY
} ;
typedef enum resolve_tree_enum_category_e resolve_tree_enum_category_t ;

typedef struct resolve_tree_enum_table_s resolve_tree_enum_table_t ;
struct resolve_tree_enum_table_s
{
    resolve_tree_enum_category_t category ;
    uint32_t id ;
    key_description_t const *list ;
} ;


#define E_TABLE_TREE_ZERO { \
    .category = E_RESOLVE_CATEGORY_TREE, \
    .u.tree.category = E_RESOLVE_TREE_CATEGORY_TREE, \
    .u.tree.list = enum_list_tree \
}

#define E_TABLE_TREE_MASTER_ZERO { \
    .category = E_RESOLVE_CATEGORY_TREE, \
    .u.tree.category = E_RESOLVE_TREE_CATEGORY_MASTER, \
    .u.tree.list = enum_list_tree_master \
}

extern const char *enum_str_tree[] ;
extern const char *enum_str_tree_master[] ;
extern key_description_t const enum_list_tree[] ;
extern key_description_t const enum_list_tree_master[] ;
extern const key_description_t *enum_get_list_tree(resolve_tree_enum_table_t category) ;

#endif