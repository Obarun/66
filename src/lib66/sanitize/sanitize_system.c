/*
 * sanitize_system.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

#include <66/ssexec.h>
#include <66/config.h>
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

int sanitize_system(ssexec_t *info)
{
    log_flow() ;

    int r ;
    size_t baselen = info->base.len ;
    uid_t log_uid ;
    gid_t log_gid ;
    char dst[baselen + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1] ;
    auto_strings(dst,info->base.s, SS_SYSTEM) ;

    /** base is /var/lib/66 or $HOME/.66*/
    /** this verification is made in case of
     * first use of 66 cmd tools */
    auto_check(dst) ;
    /** create extra directory for service part */
    if (!info->owner) {

        /** filesystem may on ro at boot.
         * only pass through chmod if it exists yet.
         * It should already exist at boot time because service
         * was enabled and so sanitize_system was run at least one time */
        if (access(SS_LOGGER_SYSDIR, F_OK) < 0) {

            auto_check(SS_LOGGER_SYSDIR) ;

            if (!youruid(&log_uid,SS_LOGGER_RUNNER) ||
                !yourgid(&log_gid,log_uid))
                    log_dieusys(LOG_EXIT_SYS,"get uid and gid of: ",SS_LOGGER_RUNNER) ;

            if (chown(SS_LOGGER_SYSDIR,log_uid,log_gid) == -1)
                log_dieusys(LOG_EXIT_SYS,"chown: ",SS_LOGGER_RUNNER) ;
        }

        auto_check(SS_SERVICE_SYSDIR) ;
        auto_check(SS_SERVICE_ADMDIR) ;
        auto_check(SS_SERVICE_ADMCONFDIR) ;
        auto_check(SS_SCRIPT_SYSDIR) ;
        auto_check(SS_SEED_ADMDIR) ;
        auto_check(SS_SEED_SYSDIR) ;

    } else {

        char home[SS_MAX_PATH_LEN + 1] ;

        if (!set_ownerhome_stack(home))
            log_dieusys(LOG_EXIT_SYS,"set home directory") ;

        size_t homelen = strlen(home) ;
        char target[homelen + SS_MAX_PATH_LEN + 1] ;

        auto_strings(target, home, SS_USER_DIR, SS_SYSTEM) ;
        auto_check(target) ;

        auto_strings(target + homelen, SS_LOGGER_USERDIR) ;
        auto_check(target) ;

        auto_strings(target + homelen, SS_SERVICE_USERDIR) ;
        auto_check(target) ;

        auto_strings(target + homelen, SS_SERVICE_USERCONFDIR) ;
        auto_check(target) ;

        auto_strings(target + homelen, SS_SCRIPT_USERDIR) ;
        auto_check(target) ;

        auto_strings(target + homelen, SS_SEED_USERDIR) ;
        auto_check(target) ;
    }

    auto_strings(dst, info->base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;
    auto_check(dst) ;

    auto_strings(dst + baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN, SS_MASTER) ;

    if (!scan_mode(dst, S_IFREG))
        if (!tree_resolve_master_create(info->base.s, info->owner))
            log_dieu(LOG_EXIT_SYS, "write Master resolve file of trees") ;

    auto_strings(dst + baselen + SS_SYSTEM_LEN, SS_SERVICE, SS_SVC) ;
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

        char const *prog = PROG ;
        PROG = "tree" ;

        if (ssexec_tree_admin(nargc, newargv, info))
            log_dieu(LOG_EXIT_SYS, "create tree: ", SS_DEFAULT_TREENAME) ;

        PROG = prog ;
    }

    return 1 ;
}
