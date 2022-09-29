/*
 * graph_build_service.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/tree.h>
#include <66/resolve.h>
#include <66/graph.h>

/**
 * @general has only effect for DATA_SERVICE type
 * !general -> all services from @treename
 * general -> all services from all trees of the system
 *
 * */
int graph_build_service(graph_t *g, char const *base, char const *treename, uint8_t general)
{
    log_flow() ;

    int e = 0 ;
    size_t pos = 0, baselen = strlen(base) ;
    stralloc sa = STRALLOC_ZERO ;

    char solve[baselen + SS_SYSTEM_LEN + 1 + SS_MAX_TREENAME + SS_SVDIRS_LEN + 1] ;

    if (general) {

        if (!resolve_get_field_tosa_g(&sa, base, treename, SS_MASTER + 1, DATA_TREE_MASTER, E_RESOLVE_TREE_MASTER_CONTENTS))
            goto err ;

        FOREACH_SASTR(&sa, pos) {

            auto_strings(solve + baselen + SS_SYSTEM_LEN + 1, sa.s + pos, SS_SVDIRS) ;

            if (!graph_build_service_bytree(g, solve, 2))
                goto err ;
        }

    } else {

        auto_strings(solve, base, SS_SYSTEM, "/", treename, SS_SVDIRS) ;

        if (!graph_build_service_bytree(g, solve, 2))
                goto err ;
    }

    e = 1 ;
    err:
        stralloc_free(&sa) ;
        return e ;
}
