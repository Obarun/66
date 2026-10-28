/*
 * tree_contents_edit.c
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

#include <stdbool.h>
#include <string.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/resolve.h>
#include <66/tree.h>

int tree_contents_edit(char const *base, char const *treename, char const *services, bool add)
{
    log_flow() ;

    int e = -1 ;
    ssize_t at ;
    size_t pos = 0 ;
    bool changed = false ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_TREE, &tres) ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;
    _cleanup_strbuf_ strbuf stk = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf names = STRBUF_ZERO ;

    int r = resolve_read(&wres, base, treename) ;
    if (r <= 0) {
        e = r ;
        goto freed ;
    }

    if ((tres.ncontents && !sbl_clean_string(&stk, tres.sa.s + tres.contents)) || !sbl_clean_string(&names, services))
        goto freed ;

    FOREACH_SBL(&names, pos) {

        at = sbl_search(&stk, names.s + pos) ;

        if (add ? at >= 0 : at < 0)
            continue ;

        if (add ? !sbl_add(&stk, names.s + pos) : !sbl_remove_at(&stk, at))
            goto freed ;

        changed = true ;
    }

    if (!changed) {
        e = 1 ;
        goto freed ;
    }

    // count first: rebuilding collapses the list into a single string
    tres.ncontents = sbl_count(&stk) ;

    if (stk.len && !sbl_rebuild_with_delim(&stk, ' '))
        goto freed ;

    table.u.tree.id = E_RESOLVE_TREE_CONTENTS ;

    if (!resolve_modify_field_by(&wres, table, stk.len ? stk.s : "") || !resolve_write(&wres, base, treename))
        goto freed ;

    e = 1 ;

    freed:
        resolve_free_obj(&wres) ;
        return e ;
}

int tree_contents_batch(char const *base, strbuf *pairs, bool add)
{
    log_flow() ;

    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf done = STRBUF_ZERO ;

    FOREACH_SBL(pairs, pos) {

        size_t at = 0 ;
        char const *tree = pairs->s + pos ;
        _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;

        pos += strlen(tree) + 1 ;

        if (sbl_search(&done, tree) >= 0)
            continue ;

        FOREACH_SBL(pairs, at) {

            char const *t = pairs->s + at ;

            at += strlen(t) + 1 ;

            if (!strcmp(t, tree) && !auto_strbuf(&list, pairs->s + at, " "))
                return -1 ;
        }

        if (!sbl_add(&done, tree))
            return -1 ;

        if (add ? tree_contents_edit(base, tree, list.s, true) <= 0 : tree_service_remove(base, tree, list.s) < 0)
            return -1 ;
    }

    return 1 ;
}
