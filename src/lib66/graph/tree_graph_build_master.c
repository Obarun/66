/*
 * tree_graph_build_master.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/graph.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/tree.h>

uint32_t tree_graph_build_master(tree_graph_t *g, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    uint32_t n = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    resolve_enum_table_t table = E_TABLE_TREE_MASTER_ZERO ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

    table.u.tree.id = E_RESOLVE_TREE_MASTER_CONTENTS ;

    if (!resolve_get_field(&sa, wres, info->base.s, SS_MASTER + 1, table))
        log_dieu(LOG_EXIT_SYS, "get resolve Master file of trees") ;

    resolve_free(wres) ;

    n = tree_graph_ncollect(g, sa.s, sa.len, info) ;
    if (!n)
        return n ;

    if (!tree_graph_nresolve(g, sa.s, sa.len, flag))
        return (errno = EINVAL, 0) ;

    return n ;
}