/*
 * tree_service_remove.c
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
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/tree.h>

void tree_service_remove(char const *base, char const *treename, char const *service)
{
    log_flow() ;

    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    stralloc sa = STRALLOC_ZERO ;
    char *str = 0 ;

    log_trace("modify field contents of resolve tree file: ", treename) ;

    if (resolve_read_g(wres, base, treename) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of tree: ", treename) ;

    if (tres.ncontents) {

        if (!sastr_clean_string(&sa, tres.sa.s + tres.contents))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

        if (!sastr_remove_element(&sa, service))
            log_dieu(LOG_EXIT_SYS, "remove service: ", service, " from selection") ;

        if (sa.len) {
            if (!sastr_rebuild_in_oneline(&sa))
                log_dieu(LOG_EXIT_SYS, "rebuild stralloc list") ;
            str = sa.s ;

        } else str = "" ;

        if (!resolve_modify_field(wres, E_RESOLVE_TREE_CONTENTS, str))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of: ", treename) ;
    }

    if (tres.ncontents)
        tres.ncontents-- ;

    if (!resolve_write_g(wres, base, treename))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of tree: ", treename) ;

    stralloc_free(&sa) ;
    resolve_free(wres) ;
}
