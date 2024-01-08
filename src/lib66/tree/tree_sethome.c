/*
 * tree_sethome.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/ssexec.h>
#include <66/tree.h>

/**
 * @Return -1 > error system
 * @Return 0 > unable to find seed file
 * @Return 1 > success */
int tree_sethome(ssexec_t *info)
{
    log_flow() ;

    char const *base = info->base.s ;

    int r ;

    if (!info->opt_tree) {

        char t[SS_MAX_TREENAME + 1] ;
        r = tree_find_current(t, base) ;

        if (r < 0)
            return -1 ;

        info->treename.len = 0 ;
        if (!r) {
            /** no current tree found. Use the default one.*/
            if (!auto_stra(&info->treename, SS_DEFAULT_TREENAME))
                return -1 ;
        } else {

            if (!auto_stra(&info->treename, t))
                return -1 ;
        }

    } else {

        r = tree_isvalid(info->base.s, info->treename.s) ;

        if (r < 0) {
            /** The tree name exist on the system
             * but it's not a directory */
             errno = EEXIST ;
             return -1 ;

        } else if (!r) {

            /** Tree doesn't exist yet.
             * Let see if we have a seed file to create it.
             * if (!tree_seed_isvalid(info->treename.s))
             *   log_warnu_return(LOG_EXIT_ZERO,"find a seed file to create the tree: ", info->treename.s) ; */

            int nargc = 3 ;
            char const *newargv[nargc] ;
            unsigned int m = 0 ;

            newargv[m++] = "tree" ;
            newargv[m++] = info->treename.s ;
            newargv[m++] = 0 ;

            char const *prog = PROG ;
            PROG = "tree" ;
            if (ssexec_tree_admin(nargc, newargv, info))
                log_warnu_return(LOG_EXIT_LESSONE,"create tree: ",info->treename.s) ;
            PROG = prog ;
        }

    }

    return 1 ;
}
