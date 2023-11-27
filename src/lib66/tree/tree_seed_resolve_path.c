/*
 * tree_seed_resolve_path.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <unistd.h>//getuid
#include <sys/types.h>

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/tree.h>

int tree_seed_resolve_path(stralloc *sa, char const *seed)
{
    log_flow() ;

    int r ;

    char *src = 0 ;
    uid_t uid = getuid() ;
    if (!uid) {

        src = SS_SEED_ADMDIR ;

    } else {

        if (!set_ownerhome(sa, uid))
            log_warnusys_return(LOG_EXIT_ZERO, "set home directory") ;

        if (!auto_stra(sa, SS_SEED_USERDIR))
            log_warnsys_return(LOG_EXIT_ZERO, "stralloc") ;

        src = sa->s ;
    }

    r = tree_seed_file_isvalid(src, seed) ;
    if (r == -1)
        return 0 ;

    if (!r) {

        /** yeah double check because we can come from !uid */
        src = SS_SEED_ADMDIR ;
        r = tree_seed_file_isvalid(src, seed) ;
        if (r == -1)
            return 0 ;

        if (!r) {

            src = SS_SEED_SYSDIR ;
            r = tree_seed_file_isvalid(src, seed) ;
            if (r != 1)
                return 0 ;

        }
    }

    sa->len = 0 ;
    if (!auto_stra(sa,src, seed))
        log_warnsys_return(LOG_EXIT_ZERO, "stralloc") ;

    return 1 ;

}
