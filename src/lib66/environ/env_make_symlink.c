/*
 * env_make_symlink.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>

#include <skalibs/unix-transactional.h>//atomic_symlink

#include <66/environ.h>
#include <66/parse.h>
#include <66/constants.h>
#include <66/service.h>

int env_make_symlink(resolve_service_t *res)
{
    log_flow() ;

    /** svconf-> /etc/66/conf/<service_name> */
    char *svconf = res->sa.s + res->environ.envdir ;
    char *version = res->sa.s + res->version ;
    size_t version_len = strlen(version), svconf_len = strlen(svconf) ;
    char sym_version[svconf_len + SS_SYM_VERSION_LEN + 1] ;

    char dst[svconf_len + 1 + version_len + 1] ;

    auto_strings(dst,svconf,"/",version) ;

    auto_strings(sym_version,svconf,SS_SYM_VERSION) ;

    log_trace("create directory: ", dst) ;
    if (!dir_create_parent(dst,0755))
        log_warnsys_return(LOG_EXIT_ZERO,"create directory: ",dst) ;

    /** atomic_symlink check if exist
     * if it doesn't exist, it create it*/
    log_trace("point symlink: ", sym_version, " to ", dst) ;
    if (!atomic_symlink(dst,sym_version,"env_compute"))
        log_warnu_return(LOG_EXIT_ZERO,"symlink: ",sym_version," to: ",dst) ;

    return 1 ;
}
