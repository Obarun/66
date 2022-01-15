/*
 * env_compute.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
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
#include <string.h>

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/environ.h>
#include <66/parser.h>


int env_compute(stralloc *result,sv_alltype *sv, uint8_t conf)
{
    log_flow() ;

    int r ;
    char *version = keep.s + sv->cname.version ;
    char *svconf = keep.s + sv->srconf ;
    char *name = keep.s + sv->cname.name ;
    size_t svconf_len = strlen(svconf), version_len = strlen(version) ;
    char src[svconf_len + 1 + version_len + 1] ;

    auto_strings(src,svconf,"/",version) ;

    /** previous version, this is the current version before
     * the switch with env_make_symlink() */
    stralloc pversion = STRALLOC_ZERO ;
    // future version, the one which we want
    stralloc fversion = STRALLOC_ZERO ;

    /** store current configure file version before the switch
     * of the symlink with the env_make_symlink() function */
    r = env_find_current_version(&pversion,svconf) ;

    if (r == -1)
        log_warnu_return(LOG_EXIT_ZERO,"find previous configuration file version") ;

    if(!env_make_symlink(sv))
        return 0 ;

    /** !r means that previous version doesn't exist, no need to import anything */
    if (r && !conf) {

        r = env_find_current_version(&fversion,svconf) ;
        /** should never happen, the env_make_symlink() die in case of error */
        if (r <= 0)
            log_warnu_return(LOG_EXIT_ZERO,"find current configuration file version") ;

        char pv[pversion.len + 1] ;
        char fv[fversion.len + 1] ;

        if (!ob_basename(pv,pversion.s))
            log_warnu_return(LOG_EXIT_ZERO,"get basename of: ",pversion.s) ;

        if (!ob_basename(fv,fversion.s))
            log_warnu_return(LOG_EXIT_ZERO,"get basename of: ",fversion.s) ;

        if (!env_import_version_file(name,svconf,pv,fv,sv->cname.itype))
            return 0 ;

    }

    if (!auto_stra(result, \
    "## [STARTWARN]\n## DO NOT MODIFY THIS FILE, IT OVERWRITTEN AT UPGRADE TIME.\n## Uses \'66-env ", \
    name,"\' command instead.\n## Or make a copy of this file at ",src,"/",name, \
    " and modify it.\n## [ENDWARN]\n",sv->saenv.s))
        log_warnu_return(LOG_EXIT_ZERO,"stralloc") ;

    stralloc_free(&pversion) ;
    stralloc_free(&fversion) ;

    return 1 ;
}
