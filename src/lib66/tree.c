/*
 * tree.c
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

#include <66/tree.h>

#include <string.h>
#include <sys/types.h>
#include <sys/unistd.h>//access
#include <sys/errno.h>

#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/sastr.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/resolve.h>

int tree_isvalid(char const *base, char const *treename)
{
    log_flow() ;

    int r ;
    size_t baselen = strlen(base), treelen = strlen(treename) ;
    char t[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + 1 + treelen + 1] ;
    auto_strings(t, base, SS_SYSTEM, SS_RESOLVE, "/", treename) ;

    r = scan_mode(t, S_IFREG) ;
    if (r < 0)
        return -1 ;
    else if (!r)
        return 0 ;

    return 1 ;
}

int tree_find_current(stralloc *tree, char const *base, uid_t owner)
{
    log_flow() ;

    ssize_t r ;
    size_t baselen = strlen(base) ;
    char pack[UID_FMT] ;

    uint32_pack(pack,owner) ;
    size_t packlen = uint_fmt(pack,owner) ;
    pack[packlen] = 0 ;

    char t[baselen + SS_TREE_CURRENT_LEN + 1 + packlen + 1 + SS_TREE_CURRENT_LEN + 1] ;
    auto_strings(t, base, SS_TREE_CURRENT, "/", pack, "/", SS_TREE_CURRENT) ;

    r = scan_mode(t,S_IFDIR) ;
    if(r <= 0)
        return 0 ;

    r = sarealpath(tree,t) ;
    if (r < 0 )
        return 0 ;

    if (!stralloc_0(tree))
        return 0 ;

    tree->len--;

    return 1 ;
}

int tree_iscurrent(char const *base, char const *treename)
{
    log_flow() ;

    int e = -1 ;
    size_t baselen = strlen(base) ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    char t[baselen + SS_SYSTEM_LEN + 1] ;

    auto_strings(t, base, SS_SYSTEM) ;

    if (!resolve_read(wres, t, SS_MASTER + 1))
        goto err ;

    if (!strcmp(tres.sa.s + tres.current, treename))
        e = 1 ;
    else
        e = 0 ;

    err:
        resolve_free(wres) ;
        return e ;
}

int tree_isinitialized(char const *live, char const *treename, uid_t owner)
{
    log_flow() ;

    int init = 0 ;

    size_t livelen = strlen(live), treelen = strlen(treename) ;

    char pack[UID_FMT] ;
    uint32_pack(pack,owner) ;
    size_t packlen = uint_fmt(pack,owner) ;
    pack[packlen] = 0 ;

    char t[livelen + SS_STATE_LEN + 1 + packlen + 1 + treelen + 6] ;
    auto_strings(t, live, SS_STATE + 1, "/", pack, "/", treename, "/init") ;

    if (!access(t, F_OK))
        init = 1 ;

    return init ;
}

int tree_isenabled(char const *base, char const *treename)
{
    log_flow() ;

    int e = -1 ;
    size_t baselen = strlen(base), pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    char solve[baselen + SS_SYSTEM_LEN + 1] ;

    auto_strings(solve, base, SS_SYSTEM) ;

    if (!resolve_read(wres, solve, SS_MASTER + 1))
        goto err ;

    if (tres.nenabled) {

        if (!sastr_clean_string(&sa, tres.sa.s + tres.enabled))
            goto err ;

        e = 0 ;

        FOREACH_SASTR(&sa, pos) {

            if (!strcmp(treename, sa.s + pos)) {
                e = 1 ;
                break ;
            }
        }
    }
    else e = 0 ;

    err:
        resolve_free(wres) ;
        stralloc_free(&sa) ;
        return e ;
}

int tree_ongroups(char const *base, char const *treename, char const *group)
{
    log_flow() ;

    int e = -1 ;
    size_t baselen = strlen(base), pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    char solve[baselen + SS_SYSTEM_LEN + 1] ;

    auto_strings(solve, base, SS_SYSTEM) ;

    if (!resolve_read(wres, solve, treename))
        goto err ;

    if (tres.ngroups) {

        if (!sastr_clean_string(&sa, tres.sa.s + tres.groups))
            goto err ;

        e = 0 ;

        FOREACH_SASTR(&sa, pos) {

            if (!strcmp(group, sa.s + pos)) {
                e = 1 ;
                break ;
            }
        }
    }
    else e = 0 ;

    err:
       resolve_free(wres) ;
       stralloc_free(&sa) ;
       return e ;
}
