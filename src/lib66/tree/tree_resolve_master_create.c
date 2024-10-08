/*
 * tree_resolve_master_create.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <string.h>
#include <pwd.h>
#include <sys/stat.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/types.h>

#include <66/tree.h>
#include <66/resolve.h>
#include <66/constants.h>

int tree_resolve_master_create(char const *base, uid_t owner)
{
    log_flow() ;

    int e = 0 ;
    struct passwd *pw = getpwuid(owner) ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

    if (!pw) {

        if (!errno)
            errno = ESRCH ;
        goto err ;
    }

    resolve_init(wres) ;
    char ownerstr[UID_FMT];
    ownerstr[uid_fmt(ownerstr, owner)] = 0 ;
    mres.name = resolve_add_string(wres, SS_MASTER + 1) ;
    mres.allow = resolve_add_string(wres, ownerstr) ; ;
    mres.nallow++ ;

    log_trace("write Master resolve file of trees") ;
    if (!resolve_write_g(wres, base, SS_MASTER + 1))
        goto err ;

    e  = 1 ;

    err:
        resolve_free(wres) ;
        return e ;
}
