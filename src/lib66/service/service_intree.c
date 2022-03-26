/*
 * service_intree.c
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

#include <stdint.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/utils.h>
#include <66/service.h>

/** @Return -1 system error
 * @Return 0 no tree exist yet
 * @Return 1 svname doesn't exist
 * @Return 2 on success
 * @Return > 2, service exist on different tree */
int service_intree(stralloc *svtree, char const *svname, char const *tree)
{
    log_flow() ;

    uint8_t found = 1, copied = 0 ;
    uid_t owner = getuid() ;
    size_t pos = 0, newlen ;
    stralloc satree = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;
    char const *exclude[3] = { SS_BACKUP + 1, SS_RESOLVE + 1, 0 } ;

    if (!set_ownersysdir(svtree,owner)) { log_warnusys("set owner directory") ; goto err ; }
    if (!auto_stra(svtree,SS_SYSTEM)) goto err ;

    if (!scan_mode(svtree->s,S_IFDIR))
    {
        found = 0 ;
        goto freed ;
    }

    if (!auto_stra(svtree,"/")) goto err ;
    newlen = svtree->len ;

    if (!stralloc_copy(&tmp,svtree)) goto err ;

    if (!sastr_dir_get(&satree, svtree->s, exclude, S_IFDIR)) {
        log_warnu("get list of trees from directory: ",svtree->s) ;
        goto err ;
    }

    if (satree.len)
    {

        FOREACH_SASTR(&satree, pos) {

            tmp.len = newlen ;
            char *name = satree.s + pos ;

            if (!auto_stra(&tmp,name,SS_SVDIRS)) goto err ;
            if (resolve_check(tmp.s,svname)) {

                if (!tree || (tree && !strcmp(name,tree))){
                    svtree->len = 0 ;
                    if (!stralloc_copy(svtree,&tmp)) goto err ;
                    copied = 1 ;
                }
                found++ ;
            }
        }
    }
    else
    {
        found = 0 ;
        goto freed ;
    }

    if (found > 2 && tree) found = 2 ;
    if (!copied) found = 1 ;
    if (!stralloc_0(svtree)) goto err ;
    freed:
    stralloc_free(&satree) ;
    stralloc_free(&tmp) ;
    return found ;
    err:
        stralloc_free(&satree) ;
        stralloc_free(&tmp) ;
        return -1 ;
}
