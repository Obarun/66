/*
 * sanitize_system.c
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
#include <unistd.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/stralloc.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/sanitize.h>
#include <66/utils.h>
#include <66/state.h>
#include <66/tree.h>

static void auto_dir(char const *dst, mode_t mode)
{
    log_flow() ;

    log_trace("create directory: ",dst) ;
    if (!dir_create_parent(dst,mode))
        log_dieusys(LOG_EXIT_SYS, "create directory: ",dst) ;
}

static void auto_check(char *dst)
{
    log_flow() ;

    int r = scan_mode(dst,S_IFDIR) ;

    if (r == -1)
        log_diesys(LOG_EXIT_SYS, "conflicting format for: ", dst) ;

    if (!r)
        auto_dir(dst,0755) ;
}

static void inline auto_stralloc(stralloc *sa,char const *str)
{
    log_flow() ;

    if (!auto_stra(sa,str))
        log_die_nomem("stralloc") ;
}

int sanitize_system(ssexec_t *info)
{
    log_flow() ;

    log_trace("sanitize system..." ) ;

    int r ;
    size_t baselen = info->base.len ;
    uid_t log_uid ;
    gid_t log_gid ;
    char dst[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + 1 + SS_SERVICE_LEN + 1] ;
    auto_strings(dst,info->base.s, SS_SYSTEM) ;

    /** base is /var/lib/66 or $HOME/.66*/
    /** this verification is made in case of
     * first use of 66 cmd tools */
    auto_check(dst) ;
    /** create extra directory for service part */
    if (!info->owner) {

        auto_check(SS_LOGGER_SYSDIR) ;

        if (!youruid(&log_uid,SS_LOGGER_RUNNER) ||
        !yourgid(&log_gid,log_uid))
            log_dieusys(LOG_EXIT_SYS,"get uid and gid of: ",SS_LOGGER_RUNNER) ;

        if (chown(SS_LOGGER_SYSDIR,log_uid,log_gid) == -1)
            log_dieusys(LOG_EXIT_SYS,"chown: ",SS_LOGGER_RUNNER) ;

        auto_check(SS_SERVICE_SYSDIR) ;
        auto_check(SS_SERVICE_ADMDIR) ;
        auto_check(SS_SERVICE_ADMCONFDIR) ;
        auto_check(SS_MODULE_SYSDIR) ;
        auto_check(SS_MODULE_ADMDIR) ;
        auto_check(SS_SCRIPT_SYSDIR) ;
        auto_check(SS_SEED_ADMDIR) ;
        auto_check(SS_SEED_SYSDIR) ;

    } else {

        size_t extralen ;
        stralloc extra = STRALLOC_ZERO ;
        if (!set_ownerhome(&extra,info->owner))
            log_dieusys(LOG_EXIT_SYS,"set home directory") ;

        extralen = extra.len ;
        if (!auto_stra(&extra, SS_USER_DIR, SS_SYSTEM))
            log_die_nomem("stralloc") ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_LOGGER_USERDIR) ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_SERVICE_USERDIR) ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_SERVICE_USERCONFDIR) ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_MODULE_USERDIR) ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_SCRIPT_USERDIR) ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_SEED_USERDIR) ;
        auto_check(extra.s) ;

        stralloc_free(&extra) ;
    }

    auto_strings(dst, info->base.s, SS_SYSTEM, SS_RESOLVE, "/", SS_SERVICE) ;
    auto_check(dst) ;

    auto_strings(dst + baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN, SS_MASTER) ;

    if (!scan_mode(dst, S_IFREG))
        if (!tree_resolve_master_create(info->base.s, info->owner))
            log_dieu(LOG_EXIT_SYS, "write Master resolve file of trees") ;

    auto_strings(dst + baselen, SS_TREE_CURRENT) ;
    auto_check(dst) ;

    /** create the default tree if it doesn't exist yet */
    r = tree_isvalid(info->base.s, SS_DEFAULT_TREENAME) ;
    if (r < 0)
        log_dieu(LOG_EXIT_SYS, "check validity of tree: ", SS_DEFAULT_TREENAME) ;

    if (!r) {

        int nargc = 4 ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "tree" ;
        newargv[m++] = "-E" ;
        newargv[m++] = SS_DEFAULT_TREENAME ;
        newargv[m++] = 0 ;

        char *prog = PROG ;
        PROG = "tree" ;

        if (ssexec_tree(nargc, newargv, info))
            log_dieu(LOG_EXIT_SYS, "create tree: ", SS_DEFAULT_TREENAME) ;

        PROG = prog ;
    }

    return 1 ;
}
