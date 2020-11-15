/*
 * tree_switch_current.c
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <66/tree.h>

#include <sys/types.h>
#include <stddef.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/unix-transactional.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/utils.h>


int tree_switch_current(char const *base, char const *treename)
{
    ssize_t r ;
    char pack[UID_FMT] ;
    size_t baselen = strlen(base) ;
    size_t treelen = strlen(treename) ;
    size_t newlen ;
    size_t packlen ;
    packlen = uint_fmt(pack,MYUID) ;
    pack[packlen] = 0 ;
    char dst[baselen + SS_TREE_CURRENT_LEN + 1 + packlen + treelen + 2 + 1] ;

    auto_strings(dst,base,SS_SYSTEM,"/",treename) ;

    r = scan_mode(dst,S_IFDIR) ;
    if (r <= 0) return 0 ;

    auto_string_from(dst,baselen,SS_TREE_CURRENT,"/",pack) ;
    newlen = baselen + SS_TREE_CURRENT_LEN + 1 + packlen ;

    r = scan_mode(dst,S_IFDIR) ;
    if (!r){
        if (!dir_create_parent(dst,0755)) return 0 ;
    }
    if(r == -1) return 0 ;

    char current[newlen + 1 + SS_TREE_CURRENT_LEN + 1] ;

    auto_strings(current,dst,"/",SS_TREE_CURRENT) ;
    auto_string_from(dst,baselen,SS_SYSTEM,"/",treename) ;

    if (!atomic_symlink(dst, current,"tree_switch_current")) return 0 ;

    return 1 ;
}
