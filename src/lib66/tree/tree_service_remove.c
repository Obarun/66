/*
 * tree_service_remove.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/lexer.h>

#include <66/resolve.h>
#include <66/tree.h>

void tree_service_remove(char const *base, char const *treename, char const *service)
{
    log_flow() ;

    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;
    char *str = 0 ;

    if (resolve_read_g(wres, base, treename) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of tree: ", treename) ;

    if (tres.ncontents) {

        size_t clen = strlen(tres.sa.s + tres.contents) ;
        _alloc_sbl_(stk, clen + 1) ;

        if (!sbl_clean_string(&stk, tres.sa.s + tres.contents))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        if (!sbl_remove(&stk, service))
            log_dieu(LOG_EXIT_SYS, "remove service: ", service, " from selection") ;

        if (stk.len) {

            if (!sbl_rebuild_with_delim(&stk, ' '))
                log_dieu(LOG_EXIT_SYS, "convert stack to string") ;

            str = stk.s ;

            tres.ncontents = sbl_count(&stk) ;

        } else {

            tres.ncontents = 0 ;
            str = "" ;
        }

        table.u.tree.id = E_RESOLVE_TREE_CONTENTS ;

        if (!resolve_modify_field(wres, table, str))
            log_dieusys(LOG_EXIT_SYS, "modify resolve file of: ", treename) ;

        if (!resolve_write_g(wres, base, treename))
            log_dieusys(LOG_EXIT_SYS, "write resolve file of tree: ", treename) ;
    }

    resolve_free(wres) ;
}
