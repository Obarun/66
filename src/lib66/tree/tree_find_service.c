/*
 * tree_find_service.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/tree.h>
#include <66/service.h>

static int trees_of(strbuf *list, char const *base)
{
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_TREE_MASTER, &mres) ;
    resolve_enum_table_t table = E_TABLE_TREE_MASTER_ZERO ;

    table.u.tree.id = E_RESOLVE_TREE_MASTER_CONTENTS ;

    int r = resolve_get_field(list, &wres, base, SS_MASTER + 1, table) ;

    resolve_free_obj(&wres) ;

    return r ;
}

static int contents_of(strbuf *list, char const *base, char const *tree)
{
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_TREE, &tres) ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;

    table.u.tree.id = E_RESOLVE_TREE_CONTENTS ;

    int r = resolve_get_field(list, &wres, base, tree, table) ;

    resolve_free_obj(&wres) ;

    return r ;
}

int tree_find_service(strbuf *tree, char const *base, char const *name)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf trees = STRBUF_ZERO ;

    r = trees_of(&trees, base) ;
    if (r <= 0)
        return r ;

    FOREACH_SBL(&trees, pos) {

        char const *treename = trees.s + pos ;
        _cleanup_strbuf_ strbuf contents = STRBUF_ZERO ;

        r = contents_of(&contents, base, treename) ;
        if (r < 0)
            return -1 ;

        if (r && sbl_search(&contents, name) >= 0)
            return auto_strbuf(tree, treename) ? 1 : -1 ;
    }

    return 0 ;
}
