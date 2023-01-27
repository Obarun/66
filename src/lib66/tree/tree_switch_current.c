/*
 * tree_switch_current.c
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

#include <string.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/unix-transactional.h>
#include <skalibs/types.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/tree.h>
#include <66/utils.h>

int tree_switch_current(char const *base, char const *treename)
{
    log_flow() ;

    int r = 0, e = 0 ;
    size_t baselen = strlen(base) ;
    size_t treelen = strlen(treename) ;

    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

    char pack[UID_FMT] ;
    size_t packlen = uint_fmt(pack, MYUID) ;
    pack[packlen] = 0 ;
    char dst[baselen + SS_SYSTEM_LEN + SS_TREE_CURRENT_LEN + 1 + packlen + treelen + 2 + 1] ;
    char sym[baselen + SS_TREE_CURRENT_LEN + 1 + packlen + 1 + SS_TREE_CURRENT_LEN + 1] ;

    auto_strings(dst, base, SS_TREE_CURRENT, "/" , pack) ;

    r = scan_mode(dst,S_IFDIR) ;
    if (!r){
        if (!dir_create_parent(dst,0755))
            goto freed ;
    } else if(r == -1)
        goto freed ;

    auto_strings(dst, base, SS_SYSTEM, "/", treename) ;

    auto_strings(sym, base, SS_TREE_CURRENT, "/", pack, "/", SS_TREE_CURRENT) ;

    if (!atomic_symlink(dst, sym,"tree_switch_current"))
        goto freed ;

    if (!resolve_modify_field_g(wres, base, SS_MASTER + 1, E_RESOLVE_TREE_MASTER_CURRENT,  treename)) {
        log_warnu("modify field: ", resolve_tree_master_field_table[E_RESOLVE_TREE_MASTER_CURRENT].field," of inner resolve file with value: ", treename) ;
        goto freed ;
    }

    e = 1 ;

    freed:
        resolve_free(wres) ;
        return e ;
}
