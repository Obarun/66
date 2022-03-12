/*
 * tree_get_permissions.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <66/utils.h>

#include <sys/types.h>

#include <oblibs/types.h>
#include <oblibs/string.h>

#include <skalibs/types.h>

#include <66/constants.h>

int tree_get_permissions(char const *tree,uid_t owner)
{
    log_flow() ;

    ssize_t r ;
    size_t treelen = strlen(tree) ;
    char pack[UID_FMT] ;
    uint32_pack(pack,owner) ;
    size_t packlen = uint_fmt(pack,owner) ;
    pack[packlen] = 0 ;

    char tmp[treelen + SS_RULES_LEN + 1 + packlen + 1] ;

    auto_strings(tmp, tree, SS_RULES, "/", pack) ;

    r = scan_mode(tmp,S_IFREG) ;
    if (r != 1) return 0 ;

    return 1 ;
}
