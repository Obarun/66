/*
 * tree_service_add.c
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
#include <66/ssexec.h>

void tree_service_add(char const *treename, char const *service, ssexec_t *info)
{
    log_flow() ;

    size_t len = strlen(service) ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;

    if (!tree_isvalid(info->base.s, treename)) {

        char const *newargv[2] = { treename, 0 } ;

        char const *prog = PROG ;
        PROG = "tree" ;
        if (ssexec_tree_admin(1, newargv, info))
            log_dieusys(LOG_EXIT_SYS, "create tree: ", treename) ;
        PROG = prog ;

    }

    if (resolve_read_g(wres, info->base.s, treename) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of tree: ", treename) ;

    _alloc_sbl_(stk, strlen(tres.sa.s + tres.contents) + len + 3) ;

    if (tres.ncontents) {

        if (!sbl_clean_string(&stk, tres.sa.s + tres.contents))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        if (sbl_search(&stk, service) < 0) {
            if (!sbl_add(&stk, service))
                log_dieusys(LOG_EXIT_SYS, "add service: ", service, " to tree: ", treename) ;
        }

    } else {

        if (!sbl_add(&stk, service))
            log_dieu(LOG_EXIT_SYS, "add string to stack") ;

    }

    tres.ncontents = sbl_count(&stk) ;

    if (!sbl_rebuild_with_delim(&stk, ' '))
        log_dieu(LOG_EXIT_SYS, "convert stack to string") ;

    table.u.tree.id = E_RESOLVE_TREE_CONTENTS ;

    if (!resolve_modify_field(wres, table, stk.s))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of tree: ", treename) ;

    if (!resolve_write_g(wres, info->base.s, treename))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of tree: ", treename) ;

    resolve_free(wres) ;
}
