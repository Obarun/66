/*
 * tree_sethome.c
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/environ.h> //char const **environ ;

#include <66/constants.h>
#include <66/ssexec.h>
#include <66/tree.h>

/**
 * @Return -3 > unable to find current
 * @Return -2 > unable to fix tree name
 * @Return -1 > unable to parse seed file
 * @Return 0 > tree doesn't exist
 * @Return 1 > success */
int tree_sethome(ssexec_t *info)
{
    log_flow() ;

    char const *base = info->base.s ;

    int r ;

    if (!info->treename.len) {

        if (!tree_find_current(&info->tree, base))
            return -3 ;

        if (!tree_setname(&info->treename,info->tree.s))
            return -2 ;

    } else {

        r = tree_isvalid(info->base.s, info->treename.s) ;

        if (r < 0) {
            /** The tree name exist on the system
             * but it's not a directory */
             errno = EEXIST ;
             return 0 ;

        } else if (!r) {
            /** Tree doesn't exist yet.
             * Let see if we have a seed/<name> to create it,
             * but only if we come from the 66-enable tool. */
            if (strcmp(info->prog, "66-enable"))
                return 0 ;

            if (!tree_seed_isvalid(info->treename.s))
                log_warnu_return(LOG_EXIT_ZERO,"find a seed file to create the tree: ", info->treename.s) ;

            int nargc = 3 ;
            char const *newargv[nargc] ;
            unsigned int m = 0 ;

            newargv[m++] = "fake_name" ;
            newargv[m++] = info->treename.s ;
            newargv[m++] = 0 ;

            if (ssexec_tree(nargc, newargv,(char const *const *)environ, info))
                log_warnu_return(LOG_EXIT_ZERO,"create tree: ",info->treename.s) ;
        }
        /** The tree_sethome() function can be recursively called. The info->tree may not empty.
         * Be sure to clean up before using it. */
        info->tree.len = 0 ;
        if (!auto_stra(&info->tree, base, SS_SYSTEM, "/", info->treename.s))
            log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    }

    return 1 ;
}
