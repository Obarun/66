/*
 * tree_service_add.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

    if (!tree_isvalid(info->base.s, treename)) {

        int nargc = 3 ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "tree" ;
        newargv[m++] = treename ;
        newargv[m++] = 0 ;

        char const *prog = PROG ;
        PROG = "tree" ;
        if (ssexec_tree_admin(nargc, newargv, info))
            log_dieusys(LOG_EXIT_SYS, "create tree: ", treename) ;
        PROG = prog ;

    }

    if (resolve_read_g(wres, info->base.s, treename) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of tree: ", treename) ;

    _alloc_stk_(stk, strlen(tres.sa.s + tres.contents) + len + 3) ;

    if (tres.ncontents) {

        if (!stack_string_clean(&stk, tres.sa.s + tres.contents))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        if (stack_retrieve_element(&stk, service) < 0) {
            if (!stack_add_g(&stk, service))
                log_dieusys(LOG_EXIT_SYS, "add service: ", service, " to tree: ", treename) ;

            if (!stack_close(&stk))
                log_dieusys(LOG_EXIT_SYS, "close stack") ;
        }

    } else {

        if (!stack_add_g(&stk, service))
            log_dieu(LOG_EXIT_SYS, "add string to stack") ;

        if (!stack_close(&stk))
            log_dieusys(LOG_EXIT_SYS, "close stack") ;

    }

    tres.ncontents = stack_count_element(&stk) ;

    if (!stack_string_rebuild_with_delim(&stk, ' '))
        log_dieu(LOG_EXIT_SYS, "convert stack to string") ;

    if (!resolve_modify_field(wres, E_RESOLVE_TREE_CONTENTS, stk.s))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of tree: ", treename) ;

    if (!resolve_write_g(wres, info->base.s, treename))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of tree: ", treename) ;

    resolve_free(wres) ;
}
