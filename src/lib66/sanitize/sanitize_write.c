/*
 * sanitize_write.c
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

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/directory.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>

#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>

#include <66/sanitize.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/tree.h>
#include <66/state.h>
#include <66/svc.h>

static void remove_dir(char const *path, char const *name)
{
    size_t pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    char const *exclude[5] = { "event", "supervise", "scandir", 0 } ;
    if (!sastr_dir_get_recursive(&sa, path, exclude, S_IFDIR|S_IFREG, 1))
        log_dieusys(LOG_EXIT_SYS, "get list of running services") ;

    FOREACH_SASTR(&sa, pos) {
        log_trace("remove element: ", sa.s + pos, " from: ", name, " service directory") ;

        if (!dir_rm_rf(sa.s + pos))
            log_dieusys(LOG_EXIT_SYS, "remove: ", sa.s + pos) ;
    }

    stralloc_free(&sa) ;
}

static void resolve_compare(resolve_service_t *res)
{
    log_flow() ;

    int r ;
    resolve_service_t fres = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &fres) ;
    ss_state_t sta = STATE_ZERO ;
    char *name = res->sa.s + res->name ;

    r = resolve_read_g(wres, res->sa.s + res->path.home, name) ;
    if (r < 0)
        log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name) ;

    if (r) {

        if (!state_read(&sta, &fres))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name) ;

        if (service_is(&sta, STATE_FLAGS_ISSUPERVISED) == STATE_FLAGS_TRUE) {

            if (fres.type != res->type)
                log_die(LOG_EXIT_SYS, "Detection of incompatible type format for supervised service: ", name, " -- current: ", get_key_by_enum(ENUM_TYPE, res->type), " previous: ", get_key_by_enum(ENUM_TYPE, fres.type), ". Please unsupervise it with '66 unsupervice ", name,"' before trying the conversion") ;
        }

        if (strcmp(res->sa.s + res->treename, fres.sa.s + fres.treename))
            tree_service_remove(fres.sa.s + fres.path.home, fres.sa.s + fres.treename, name) ;
    }

    resolve_free(wres) ;
}

static int delete(resolve_service_t *res, uint8_t force)
{
    log_flow() ;

    int r ;

    char dir[strlen(res->sa.s + res->path.servicedir) + 1] ;

    auto_strings(dir, res->sa.s + res->path.servicedir) ;

    r = scan_mode(dir, S_IFDIR) ;
    if (r < 0)
        log_die(LOG_EXIT_SYS, "unvalid source: ", dir) ;

    if (r) {

        if (force) {

            stralloc sa = STRALLOC_ZERO ;
            ss_state_t sta = STATE_ZERO ;

            /** Keep the current symlink target. */
            if (sareadlink(&sa, dir) < 0 ||
                !stralloc_0(&sa))
                    log_dieusys(LOG_EXIT_SYS, "readlink: ", dir) ;

            resolve_compare(res) ;

            if (state_check(res)) {

                if (!state_read(&sta, res))
                    log_dieu(LOG_EXIT_SYS, "read state file of: ", res->sa.s + res->name) ;
            }

            remove_dir(sa.s, res->sa.s + res->name) ;

            if (!state_write(&sta, res))
                log_dieu(LOG_EXIT_SYS, "write state file of: ", res->sa.s + res->name) ;

            stralloc_free(&sa) ;

        } else
            /** This info should never be executed as long as the parse_frontend
             * check already verifies the service and prevents reaching this point if !force. */
            log_info_return(2, "Ignoring: ", res->sa.s + res->name, " -- service already written") ;
    }

    return 1 ;
}

static void make_servicedir(resolve_service_t *res)
{
    log_flow() ;

    int r ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name), homelen = strlen(res->sa.s + res->path.home) ;
    char sym[homelen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + 1] ;
    char dst[homelen + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + SS_RESOLVE_LEN + 1 + namelen + 1] ;

    auto_strings(sym, res->sa.s + res->path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    auto_strings(dst, res->sa.s + res->path.home, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;

    struct stat st ;
    r = lstat(sym, &st) ;
    if (r < 0) {

        if (errno != ENOENT) {

            log_dieusys(LOG_EXIT_SYS, "lstat: ", sym) ;

        } else {

            log_trace("symlink: ", sym, " to: ", dst) ;
            r = symlink(dst, sym) ;
            if (r < 0 && errno != EEXIST)
                log_dieusys(LOG_EXIT_SYS, "point symlink: ", sym, " to: ", dst) ;
        }
    }

    auto_strings(dst + homelen + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + 1 + namelen, SS_RESOLVE) ;

    r = scan_mode(dst, S_IFDIR) ;
    if (r < 0)
        log_die(LOG_EXIT_SYS, "unvalid source: ", dst) ;

    log_trace("create directory: ", dst) ;
    if (!dir_create_parent(dst, 0755))
        log_dieusys(LOG_EXIT_SYS, "create directory: ", dst) ;
}

void sanitize_write(resolve_service_t *res, uint8_t force)
{
    log_flow() ;

    if (delete(res, force) > 1)
        return ;

    make_servicedir(res) ;

    if (!service_resolve_write(res))
        log_dieu(LOG_EXIT_SYS, "write resolve file of service: ", res->sa.s + res->name) ;
}

