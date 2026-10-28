/*
 * tree_service_add.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdbool.h>

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/tree.h>
#include <66/ssexec.h>

int tree_service_add(char const *treename, char const *service, ssexec_t *info)
{
    log_flow() ;

    int r = tree_isvalid(info->base.s, treename) ;
    if (r < 0)
        return -1 ;

    if (!r) {

        char const *newargv[2] = { treename, 0 } ;

        char const *prog = PROG ;
        PROG = "tree" ;
        r = ssexec_tree_admin(1, newargv, info) ;
        PROG = prog ;

        if (r) {
            log_warnu("create tree: ", treename) ;
            return -1 ;
        }
    }

    return tree_contents_edit(info->base.s, treename, service, true) <= 0 ? -1 : 1 ;
}
