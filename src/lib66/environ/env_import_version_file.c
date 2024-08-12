/*
 * env_import_version_file.c
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

#include <sys/stat.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/environ.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/utils.h>


int env_import_version_file(char const *svname, char const *svconf, char const *sversion, char const *dversion, int svtype)
{
    log_flow() ;

    int r ;
    struct stat st ;
    size_t pos = 0, svname_len = strlen(svname) ;
    stralloc salist = STRALLOC_ZERO ;
    stralloc src_ver = STRALLOC_ZERO ;
    stralloc dst_ver = STRALLOC_ZERO ;

    char svname_dot[svname_len + 1 + 1] ;

    auto_strings(svname_dot,".",svname) ;

    r = version_compare(sversion,dversion,SS_SERVICE_VERSION_NDOT) ;

    if (!r) {

        log_warn_return(LOG_EXIT_ONE,"same configuration file version for: ",svname," -- nothing to import") ;
        goto freed ;
    }

    if (r == -2)
        log_warn_return(LOG_EXIT_ZERO,"compare ",svname," version: ",sversion," vs: ",dversion) ;

    if (r == 1) {

        log_warn_return(LOG_EXIT_ONE,"configuration file version regression for ",svname," -- ignoring importation request") ;
        goto freed ;
    }

    if (!env_append_version(&src_ver,svconf,sversion) ||
        !env_append_version(&dst_ver,svconf,dversion))
        return 0 ;

    char const *exclude[2] = { svname_dot, 0 } ;
    if (!sastr_dir_get(&salist,src_ver.s,exclude,S_IFREG))
        log_warnusys_return(LOG_EXIT_ZERO,"get configuration file from directory: ",src_ver.s) ;

    FOREACH_SASTR(&salist,pos) {

        char *name = salist.s + pos ;
        size_t namelen = strlen(name) ;

        char s[src_ver.len + 1 + namelen + 1] ;
        auto_strings(s,src_ver.s,"/",name) ;

        char d[dst_ver.len + 1 + namelen + 1] ;
        auto_strings(d,dst_ver.s,"/",name) ;

        if (lstat(s, &st) < 0)
            log_warnusys_return(LOG_EXIT_ZERO,"stat: ",s) ;

        log_info("Imports ",svname," configuration file from: ",s," to: ",d) ;

        if (!filecopy_unsafe(s, d, st.st_mode))
            log_warnusys_return(LOG_EXIT_ZERO,"copy: ", s," to: ",d) ;
    }

    /** A module type can have multiple sub-modules. Copy these directories
     * to keep trace of previous configuration file for a sub-module.
     * If we don't copy these directories, when the sub-module is parsed
     * the previous configuration doesn't exist and so this function do not
     * import anything */

    if (svtype == TYPE_MODULE) {

        salist.len = 0 ;
        pos = 0 ;

        char const *exclude[1] = { 0 } ;
        if (!sastr_dir_get(&salist,src_ver.s,exclude,S_IFDIR))
            log_warnusys_return(LOG_EXIT_ZERO,"get configuration directories from directory: ",src_ver.s) ;

        FOREACH_SASTR(&salist,pos) {

            char *name = salist.s + pos ;
            size_t namelen = strlen(name) ;

            char s[src_ver.len + 1 + namelen + 1] ;
            auto_strings(s,src_ver.s,"/",name) ;

            char d[dst_ver.len + 1 + namelen + 1] ;
            auto_strings(d,dst_ver.s,"/",name) ;

            log_info("Imports ",svname," configuration file from: ",s," to: ",d) ;

            if (!hiercopy(s,d))
                log_warnusys_return(LOG_EXIT_ZERO,"copy: ",s," to: ",d) ;
        }

    }

    freed:
    stralloc_free(&src_ver) ;
    stralloc_free(&dst_ver) ;
    stralloc_free(&salist) ;

    return 1 ;
}
