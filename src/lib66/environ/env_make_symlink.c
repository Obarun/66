/*
 * env_make_symlink.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>

#include <skalibs/unix-transactional.h>//atomic_symlink

#include <66/environ.h>
#include <66/parser.h>
#include <66/constants.h>

int env_make_symlink(sv_alltype *sv)
{
    log_flow() ;

    /** svconf-> /etc/66/conf/<service_name> */
    char *svconf = keep.s + sv->srconf ;
    char *version = keep.s + sv->cname.version ;
    size_t version_len = strlen(version), svconf_len = strlen(svconf) ;
    char sym_version[svconf_len + SS_SYM_VERSION_LEN + 1] ;

    char dst[svconf_len + 1 + version_len + 1] ;

    auto_strings(dst,svconf,"/",version) ;

    auto_strings(sym_version,svconf,SS_SYM_VERSION) ;

    if (!dir_create_parent(dst,0755))
        log_warnsys_return(LOG_EXIT_ZERO,"create directory: ",dst) ;

    /** atomic_symlink check if exist
     * if it doesn't exist, it create it*/
    if (!atomic_symlink(dst,sym_version,"env_compute"))
        log_warnu_return(LOG_EXIT_ZERO,"symlink: ",sym_version," to: ",dst) ;

    return 1 ;
}
