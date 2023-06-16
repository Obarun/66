/*
 * tree_service_add.c
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

#include <oblibs/log.h>
#include <oblibs/stack.h>

#include <66/resolve.h>
#include <66/tree.h>

void tree_service_add(char const *base, char const *treename, char const *service)
{
    log_flow() ;

    int r ;
    size_t len = strlen(service) ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    stack_ref pstk = 0 ;

    _init_stack_(stk, SS_MAX_SERVICE * SS_MAX_SERVICE_NAME + 1 + len + 1) ;

    if (resolve_read_g(wres, base, treename) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of tree: ", treename) ;

    if (tres.ncontents) {

        if (!stack_convert_string(&stk, tres.sa.s + tres.contents, strlen(tres.sa.s + tres.contents)))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        if (stack_retrieve_element(&stk, service) < 0) {

            if (!stack_add_g(&stk, service))
                log_dieusys(LOG_EXIT_SYS, "add service: ", service, " to tree: ", treename) ;

            tres.ncontents++ ;
        }

        if (!stack_close(&stk))
            log_dieusys(LOG_EXIT_SYS, "close stack") ;

    } else {

        if (!stack_add_g(&stk, service))
            log_dieu(LOG_EXIT_SYS, "add string to stack") ;

        tres.ncontents++ ;

        if (!stack_close(&stk))
            log_dieusys(LOG_EXIT_SYS, "close stack") ;

    }

    if (!stack_convert_tostring(&stk))
        log_dieu(LOG_EXIT_SYS, "convert stack to string") ;

    if (!resolve_modify_field(wres, E_RESOLVE_TREE_CONTENTS, stk.s))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of: ", treename) ;

    if (!resolve_write_g(wres, base, treename))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of tree: ", treename) ;

    resolve_free(wres) ;
}
