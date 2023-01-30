/*
 * info_graph_display_tree.c
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
 * */

#include <unistd.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/buffer.h>

#include <66/tree.h>
#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/info.h>

int info_graph_display_tree(char const *name)
{
    log_flow() ;

    int err = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

    uid_t owner = getuid() ;

    if (!set_ownersysdir(&sa, owner)) {
        log_warnusys("set owner directory") ;
        resolve_free(wres) ;
        stralloc_free(&sa) ;
        return err ;
    }

    char base[sa.len + 1] ;
    auto_strings(base, sa.s) ;

    if (tree_isvalid(sa.s, name) <= 0) {
        log_warn("invalide tree: ", name) ;
        goto freed ;
    }

    if (!resolve_read_g(wres, sa.s, name))
        goto freed ;

    sa.len = 0 ;

    int r = set_livedir(&sa) ;
    if (r <= 0)
        goto freed ;

    int init = tres.init ;
    if (init < 0)
        goto freed ;

    int up = tres.supervised ;
    if (up < 0)
        goto freed ;

    int enabled = tree_isenabled(base, name) ;
    if (enabled < 0)
        goto freed ;

    if (!bprintf(buffer_1,"%s (%s%s%s,%s%s%s,%s%s%s)", \

                name, \

                init ? log_color->valid : log_color->warning, \
                init ? "Initialized" : "Unitialized", \
                log_color->off, \

                up ? log_color->valid : log_color->error, \
                up ? "Up" : "Down", \
                log_color->off, \


                enabled ? log_color->valid : log_color->warning, \
                enabled ? "Enabled" : "Disabled", \
                log_color->off))

                    goto freed ;

    err = 1 ;

    freed:
        resolve_free(wres) ;
        stralloc_free(&sa) ;

    return err ;

}
