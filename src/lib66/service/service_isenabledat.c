/*
 * service_isenabledat.c
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

#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/utils.h>
#include <66/service.h>

/** @Return 0 if not found
 * @Return 1 if found
 * @Return 2 if found but marked disabled
 * @Return -1 system error */
int service_isenabledat(stralloc *tree, char const *sv)
{

    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    size_t newlen = 0, pos = 0 ;
    int e = -1 ;
    char const *exclude[3] = { SS_BACKUP + 1, SS_RESOLVE + 1, 0 } ;

    if (!set_ownersysdir(&sa, getuid())) {

        log_warnusys("set owner directory") ;
        stralloc_free(&sa) ;
        return 0 ;
    }

    char tmp[sa.len + SS_SYSTEM_LEN + 2] ;
    auto_strings(tmp, sa.s, SS_SYSTEM) ;

    // no tree exist yet
    if (!scan_mode(tmp, S_IFDIR))
        goto empty ;

    auto_strings(tmp, sa.s, SS_SYSTEM, "/") ;

    newlen = sa.len + SS_SYSTEM_LEN + 1 ;
    sa.len = 0 ;

    if (!sastr_dir_get(&sa, tmp, exclude, S_IFDIR)) {

        log_warnu("get list of trees from: ", tmp) ;
        goto freed ;
    }

    FOREACH_SASTR(&sa, pos) {

        char *treename = sa.s + pos ;

        char trees[newlen + strlen(treename) + SS_SVDIRS_LEN + 1] ;
        auto_strings(trees, tmp, treename, SS_SVDIRS) ;

        if (resolve_check(trees, sv)) {

            if (!resolve_read(wres, trees, sv)) {

                log_warnu("read resolve file: ", trees, "/", sv) ;
                goto freed ;
            }

            if (res.disen) {

                log_trace(sv, " enabled at tree: ", treename) ;
                e = 1 ;

            } else {

                log_trace(sv, " disabled at tree: ", treename) ;
                e = 2 ;
            }

            if (!auto_stra(tree, treename)) {
                e = -1 ;
                goto freed ;
            }
            goto freed ;
        }
    }
    empty:
        e = 0 ;
    freed:
        stralloc_free(&sa) ;
        resolve_free(wres) ;
        return e ;
}
