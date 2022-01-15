/*
 * graph_build_tree.c
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
#include <sys/stat.h>

#include <oblibs/graph.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/tree.h>


int graph_build_tree(graph_t *g, char const *base)
{
    log_flow() ;

    int e = 0 ;
    size_t baselen = strlen(base), pos = 0 ;
    char const *exclude[2] = { SS_MASTER + 1, 0 } ;
    char solve[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + 1] ;

    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

    auto_strings(solve, base, SS_SYSTEM, SS_RESOLVE) ;

    if (!sastr_dir_get(&sa, solve, exclude, S_IFREG))
        goto err ;

    solve[baselen + SS_SYSTEM_LEN] = 0 ;

    FOREACH_SASTR(&sa, pos) {

        char *name = sa.s + pos ;

        if (!resolve_read(wres, solve, name))
            goto err ;

        if (!graph_vertex_add(g, name))
            goto err ;

        if (tres.ndepends)
            if (!graph_add_deps(g, name, tres.sa.s + tres.depends, 0))
                goto err ;

        if (tres.nrequiredby)
            if (!graph_add_deps(g, name, tres.sa.s + tres.requiredby, 1))
                goto err ;
    }

    e = 1 ;

    err:
        resolve_free(wres) ;
        stralloc_free(&sa) ;
        return e ;
}
