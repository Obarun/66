/*
 * environ.c
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

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>//rename
#include <errno.h>
#include <sys/stat.h>

#include <oblibs/environ.h>
#include <oblibs/sastr.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/unix-transactional.h>//atomic_symlink
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>//rm_rf

#include <66/constants.h>
#include <66/utils.h>
#include <66/environ.h>
#include <66/enum.h>

int env_resolve_conf(stralloc *env,char const *svname, uid_t owner)
{
    log_flow() ;

    if (!owner)
    {
        if (!stralloc_cats(env,SS_SERVICE_ADMCONFDIR)) return 0 ;
    }
    else
    {
        if (!set_ownerhome(env,owner)) return 0 ;
        if (!stralloc_cats(env,SS_SERVICE_USERCONFDIR)) return 0 ;
    }
    if (!auto_stra(env,svname)) return 0 ;
    return 1 ;
}

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

int env_clean_with_comment(stralloc *sa)
{
    log_flow() ;

    ssize_t pos = 0, r ;
    char *end = 0, *start = 0 ;
    stralloc final = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;

    if (!sastr_split_string_in_nline(sa))
        log_warnu_return(LOG_EXIT_ZERO,"split environment value") ;

    for (; pos < sa->len ; pos += strlen(sa->s + pos) + 1)
    {
        tmp.len = 0 ;
        if (!sastr_clean_string(&tmp,sa->s + pos))
            log_warnu_return(LOG_EXIT_ZERO,"clean environment string") ;
        /** keep a empty line between key=value pair and a comment */
        r = get_len_until(tmp.s,'=') ;
        end = r < 0 ? "\n" : "\n\n" ;
        start = r < 0 ? "" : "\n" ;

        if (tmp.s[0] == '#')
        {
            if (!sastr_rebuild_in_oneline(&tmp))
                log_warnu_return(LOG_EXIT_ZERO,"rebuild environment string in one line") ;
        }
        else
        {
            if (!environ_rebuild_line(&tmp))
                log_warnu_return(LOG_EXIT_ZERO,"rebuild environment line") ;
        }
        if (!stralloc_0(&tmp) ||
        !auto_stra(&final,start,tmp.s,end))
            log_warn_return(LOG_EXIT_ZERO,"stralloc") ;
    }
    sa->len = 0 ;
    if (!auto_stra(sa,final.s))
        log_warnu_return(LOG_EXIT_ZERO,"store environment value") ;

    stralloc_free(&tmp) ;
    stralloc_free(&final) ;

    return 1 ;
}

int env_prepare_for_write(stralloc *name, stralloc *dst, stralloc *contents, sv_alltype *sv,uint8_t conf)
{
    log_flow() ;

    char *svconf = keep.s + sv->srconf ;
    size_t svconf_len = strlen(svconf) ;
    char sym[svconf_len + SS_SYM_VERSION_LEN + 1] ;

    auto_strings(sym,svconf,SS_SYM_VERSION) ;

    if (!env_compute(contents,sv,conf))
        log_warnu_return(LOG_EXIT_ZERO,"compute environment") ;

    if (sareadlink(dst, sym) == -1)
        log_warnusys_return(LOG_EXIT_ZERO,"read link of: ",sym) ;

    if (!stralloc_0(dst))
        log_warnusys_return(LOG_EXIT_ZERO,"stralloc") ;

    if (!auto_stra(name,".",keep.s + sv->cname.name))
        log_warnusys_return(LOG_EXIT_ZERO,"stralloc") ;

    return 1 ;
}

int env_find_current_version(stralloc *sa,char const *svconf)
{
    log_flow() ;

    size_t svconflen = strlen(svconf) ;
    struct stat st ;
    char tmp[svconflen + SS_SYM_VERSION_LEN + 1] ;

    auto_strings(tmp,svconf,SS_SYM_VERSION) ;

    /** symlink may no exist yet e.g first activation of the service */
    if (lstat(tmp,&st) == -1)
        return 0 ;

    if (sareadlink(sa,tmp) == -1)
        return -1 ;

    if (!stralloc_0(sa))
        log_warnusys_return(LOG_EXIT_ZERO,"stralloc") ;

    return 1 ;
}

int env_check_version(stralloc *sa, char const *version)
{
    log_flow() ;

    int r ;

    r = version_scan(sa,version,SS_CONFIG_VERSION_NDOT) ;

    if (r == -1)
        log_warnusys_return(LOG_EXIT_ZERO,"stralloc") ;

    if (!r)
        log_warn_return(LOG_EXIT_ZERO,"invalid version format: ",version) ;

    return 1 ;
}

int env_append_version(stralloc *saversion, char const *svconf, char const *version)
{
    log_flow() ;

    int r ;

    stralloc sa = STRALLOC_ZERO ;

    if (!env_check_version(&sa,version))
        return 0 ;

    saversion->len = 0 ;

    if (!auto_stra(saversion,svconf,"/",sa.s))
        log_warnusys_return(LOG_EXIT_ZERO,"stralloc") ;

    r = scan_mode(saversion->s,S_IFDIR) ;
    if (r == -1 || !r)
        log_warnusys_return(LOG_EXIT_ZERO,"find the versioned directory: ",saversion->s) ;

    stralloc_free(&sa) ;

    return 1 ;
}

int env_import_version_file(char const *svname, char const *svconf, char const *sversion, char const *dversion, int svtype)
{
    log_flow() ;

    int r ;
    struct stat st ;
    size_t pos = 0, svname_len= strlen(svname) ;
    stralloc salist = STRALLOC_ZERO ;
    stralloc src_ver = STRALLOC_ZERO ;
    stralloc dst_ver = STRALLOC_ZERO ;

    char svname_dot[svname_len + 1 + 1] ;

    auto_strings(svname_dot,".",svname) ;

    r = version_cmp(sversion,dversion,SS_CONFIG_VERSION_NDOT) ;

    if (!r) {

        log_warn_return(LOG_EXIT_ONE,"same configuration file version for: ",svname," -- nothing to import") ;
        goto freed ;
    }

    if (r == -2)
        log_warn_return(LOG_EXIT_ZERO,"compare ",svname," version: ",sversion," vs: ",dversion) ;

    if (r == 1) {

        log_warn_return(LOG_EXIT_ONE,"configuration file version regression for ",svname," -- ignoring configuration file version importation") ;
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

        log_info("imports ",svname," configuration file from: ",s," to: ",d) ;

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

            log_info("imports ",svname," configuration file from: ",s," to: ",d) ;

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
