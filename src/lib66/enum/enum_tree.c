/*
 * enum_tree.c
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
#include <66/enum_tree.h>

const char *enum_str_tree[] = {
    TREE_TEMPLATE(STR_TREE),
    0
} ;

const char *enum_str_tree_master[] = {
    MASTER_TEMPLATE(STR_MASTER),
    0
} ;

key_description_t const enum_list_tree[] = {
    TREE_TEMPLATE(KEY_TREE),
    { .name = 0 }
} ;

key_description_t const enum_list_tree_master[] = {
    MASTER_TEMPLATE(KEY_MASTER),
    { .name = 0 }
} ;

key_description_t const *enum_get_list_tree(resolve_tree_enum_table_t table)
{
    switch (table.category) {
        case E_RESOLVE_TREE_CATEGORY_TREE:
            return enum_list_tree ;
        case E_RESOLVE_TREE_CATEGORY_MASTER:
            return enum_list_tree_master ;
        default:
            errno = EINVAL ;
            return NULL ;
    }
}
