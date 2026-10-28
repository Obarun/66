/*
 * sanitize_system.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/strbuf.h>
#include <oblibs/files.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/constants.h>
#include <66/sanitize.h>
#include <66/utils.h>
#include <66/tree.h>
#include <66/graph.h>

static int check_dir(char const *dst, bool create, bool *missing)
{
    ssize_t r = scan_mode(dst, FLAGS_DIR) ;

    if (r < 0)
        return (errno = ENOTDIR, -1) ;

    if (r)
        return 1 ;

    if (errno != ENOENT)
        return -1 ;

    if (!create) {
        *missing = true ;
        return 1 ;
    }

    log_trace("create directory: ", dst) ;

    return dir_create_parent(dst, 0755) ? 1 : -1 ;
}

static int check_logdir(bool create, bool *missing)
{
    uid_t log_uid ;
    gid_t log_gid ;

    if (!access(SS_LOGGER_SYSDIR, F_OK))
        return 1 ;

    if (!create) {
        *missing = true ;
        return 1 ;
    }

    if (check_dir(SS_LOGGER_SYSDIR, create, missing) < 0)
        return -1 ;

    if (!youruid(&log_uid, SS_LOGGER_RUNNER) || !yourgid(&log_gid, log_uid))
        return -1 ;

    return chown(SS_LOGGER_SYSDIR, log_uid, log_gid) < 0 ? -1 : 1 ;
}

static int walk(ssexec_t *info, bool create, bool *missing)
{
    int r ;
    size_t baselen = info->base.len ;
    char dst[baselen + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + SS_PROVIDE_LEN + 1] ;

    /** base is /var/lib/66 or $HOME/.66: a first use of 66 */
    auto_strings(dst, info->base.s, SS_SYSTEM) ;
    if (check_dir(dst, create, missing) < 0)
        return -1 ;

    if (!info->owner) {

        static char const *const dirs[] = {
            SS_SERVICE_SYSDIR, SS_SERVICE_SYSDIR_USER, SS_SERVICE_ADMDIR, SS_SERVICE_ADMDIR_USER,
            SS_SERVICE_ADMCONFDIR, SS_SCRIPT_SYSDIR, SS_SEED_ADMDIR, SS_SEED_SYSDIR, SS_ENVIRONMENT_ADMDIR, 0
        } ;

        if (check_logdir(create, missing) < 0)
            return -1 ;

        for (size_t i = 0 ; dirs[i] ; i++)
            if (check_dir(dirs[i], create, missing) < 0)
                return -1 ;

    } else {

        char home[SS_MAX_PATH_LEN + 1] ;

        if (!set_ownerhome_stack(home))
            return -1 ;

        static char const *const dirs[] = {
            SS_LOGGER_USERDIR, SS_SERVICE_USERDIR, SS_SERVICE_USERCONFDIR, SS_SCRIPT_USERDIR,
            SS_SEED_USERDIR, SS_ENVIRONMENT_USERDIR, 0
        } ;
        size_t homelen = strlen(home) ;
        char target[homelen + SS_MAX_PATH_LEN + 1] ;

        auto_strings(target, home, SS_USER_DIR, SS_SYSTEM) ;
        if (check_dir(target, create, missing) < 0)
            return -1 ;

        for (size_t i = 0 ; dirs[i] ; i++) {
            auto_strings(target + homelen, dirs[i]) ;
            if (check_dir(target, create, missing) < 0)
                return -1 ;
        }
    }

    auto_strings(dst, info->base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;
    if (check_dir(dst, create, missing) < 0)
        return -1 ;

    auto_strings(dst + baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN, SS_PROVIDE) ;
    if (check_dir(dst, create, missing) < 0)
        return -1 ;

    auto_strings(dst + baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN, SS_MASTER) ;

    r = scan_mode(dst, FLAGS_REG) ;
    if (!r && errno != ENOENT)
        return -1 ;

    if (!r) {

        if (!create)
            *missing = true ;
        else if (!tree_resolve_master_create(info->base.s, info->owner))
            return -1 ;
    }

    auto_strings(dst + baselen + SS_SYSTEM_LEN, SS_SERVICE, SS_SVC) ;
    if (check_dir(dst, create, missing) < 0)
        return -1 ;

    /** the default tree */
    r = tree_isvalid(info->base.s, SS_DEFAULT_TREENAME) ;
    if (r < 0)
        return -1 ;

    if (!r) {

        if (!create) {
            *missing = true ;
            return 1 ;
        }

        extern opt_on_option_fn on_tree_admin ;

        char const *newargv[2] = { SS_DEFAULT_TREENAME, 0 } ;
        char const *prog = PROG ;
        PROG = "tree" ;

        /* enable the default tree: preset the -E action then run the handler. */
        on_tree_admin('E', 0, info) ;

        r = ssexec_tree_admin(1, newargv, info) ;

        PROG = prog ;

        if (r)
            return (errno = EINVAL, -1) ;
    }

    return 1 ;
}

int sanitize_system(ssexec_t *info)
{
    log_flow() ;

    int r, e ;
    bool missing = false ;

    if (walk(info, false, &missing) < 0)
        return -1 ;

    if (!missing)
        return 1 ;

    if (service_graph_write_begin(info->owner, info->base.s, SS_LIVE) < 0)
        return -1 ;

    r = walk(info, true, &missing) ;

    e = errno ;
    int end = service_graph_write_end(0) ;

    if (end < 0 && r > 0)
        return -1 ;

    if (!end)
        log_warnusys("write the cache of the graph of the system") ;

    errno = e ;

    return r < 0 ? -1 : 1 ;
}
