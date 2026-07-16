/*
 * tree_graph_build_groups.c
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
#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/graph.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/tree.h>

uint32_t tree_graph_build_groups(tree_graph_t *g, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    uint32_t n = 0 ;
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf selection = STRBUF_ZERO ;
    resolve_enum_table_t table = E_TABLE_TREE_MASTER_ZERO ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

    char const *group = info->treename.s ;

    table.u.tree.id = E_RESOLVE_TREE_MASTER_CONTENTS ;

    if (!resolve_get_field(&sa, wres, info->base.s, SS_MASTER + 1, table))
        log_dieu(LOG_EXIT_SYS, "get resolve Master file of trees") ;

    resolve_free(wres) ;

    FOREACH_SBL(&sa, pos) {

        int r = tree_ongroups(info->base.s, sa.s + pos, group) ;
        if (r < 0)
            return (errno = EINVAL, 0) ;

        if (!r)
            continue ;

        if (!sbl_add(&selection, sa.s + pos))
            log_die_nomem("strbuf") ;
    }

    if (!selection.len)
        // no tree in the group -- errno cleared so the caller tells a no-op from an error
        return (errno = 0, 0) ;

    n = tree_graph_ncollect(g, selection.s, selection.len, info) ;
    if (!n)
        return n ;

    if (!tree_graph_nresolve(g, selection.s, selection.len, flag))
        return (errno = EINVAL, 0) ;

    return n ;
}
