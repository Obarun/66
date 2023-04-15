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
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/tree.h>

void tree_service_add(char const *base, char const *treename, char const *service)
{
    log_flow() ;

    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    stralloc sa = STRALLOC_ZERO ;

    if (!resolve_read_g(wres, base, treename))
        log_dieusys(LOG_EXIT_SYS, "read resolve file of tree: ", treename) ;

    if (tres.ncontents) {

        if (!sastr_clean_string(&sa, tres.sa.s + tres.contents))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

        if (!sastr_add_string(&sa, service))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

        if (!sastr_sortndrop_element(&sa))
            log_dieu(LOG_EXIT_SYS, "sort string") ;

        tres.ncontents = sastr_nelement(&sa) ;

    } else {

        if (!sastr_add_string(&sa, service))
            log_dieu(LOG_EXIT_SYS, "add string") ;

        tres.ncontents++ ;
    }

    if (!sastr_rebuild_in_oneline(&sa))
        log_dieu(LOG_EXIT_SYS, "rebuild stralloc list") ;

    if (!resolve_modify_field(wres, E_RESOLVE_TREE_CONTENTS, sa.s))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of: ", treename) ;

    if (!resolve_write_g(wres, base, treename))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of tree: ", treename) ;

    stralloc_free(&sa) ;
    resolve_free(wres) ;
}
