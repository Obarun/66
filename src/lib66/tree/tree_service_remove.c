/*
 * tree_service_remove.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <oblibs/stack.h>

#include <66/resolve.h>
#include <66/tree.h>

void tree_service_remove(char const *base, char const *treename, char const *service)
{
    log_flow() ;

    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    char *str = 0 ;

    if (resolve_read_g(wres, base, treename) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of tree: ", treename) ;

    if (tres.ncontents) {

        size_t clen = strlen(tres.sa.s + tres.contents) ;
        _init_stack_(stk, clen + 1) ;

        if (!stack_clean_string(&stk, tres.sa.s + tres.contents, clen))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        if (!stack_remove_element_g(&stk, service))
            log_dieu(LOG_EXIT_SYS, "remove service: ", service, " from selection") ;

        if (stk.len) {

            if (!stack_convert_tostring(&stk))
                log_dieu(LOG_EXIT_SYS, "convert stack to string") ;

            str = stk.s ;

            tres.ncontents = stack_count_element(&stk) ;

        } else {

            tres.ncontents = 0 ;
            str = "" ;
        }

        if (!resolve_modify_field(wres, E_RESOLVE_TREE_CONTENTS, str))
            log_dieusys(LOG_EXIT_SYS, "modify resolve file of: ", treename) ;

        if (!resolve_write_g(wres, base, treename))
            log_dieusys(LOG_EXIT_SYS, "write resolve file of tree: ", treename) ;
    }

    resolve_free(wres) ;
}
