/*
 * sanitize_livestate.c
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
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/unix-transactional.h>
#include <skalibs/posixplz.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/sanitize.h>
#include <66/service.h>
#include <66/utils.h>
#include <66/state.h>

/** creation of the /run/66/state/<uid> directory */
static int sanitize_livestate_directory(resolve_service_t *res)
{
    log_flow() ;

    int r ;
    gid_t gidowner ;
    size_t livelen = strlen(res->sa.s + res->live.livedir) ;
    size_t ownerlen = strlen(res->sa.s + res->ownerstr) ;
    char ste[livelen + SS_STATE_LEN + 1 + ownerlen + 1] ;

    auto_strings(ste, res->sa.s + res->live.livedir, SS_STATE + 1, "/", res->sa.s + res->ownerstr) ;

    r = scan_mode(ste, S_IFDIR) ;
    if (r < 0)
        log_warn_return(LOG_EXIT_ZERO, "conflicting format for: ", ste) ;
    if (!r) {

        log_trace("create directory: ", ste) ;
        /** Permissions is set to 0755 to be able
         * to launch services dropping privilegies
         * before executing the run.user script.
         * Setting 0700 do not allow other users
         * to execute the script. */
        r = dir_create_parent(ste, 0755) ;
        if (!r)
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", ste) ;

        if (!yourgid(&gidowner, res->owner))
            log_warnusys_return(LOG_EXIT_ZERO, "get gid of: ", res->sa.s + res->ownerstr) ;

        if (chown(ste, res->owner, gidowner) < 0)
            log_warnusys_return(LOG_EXIT_ZERO, "chown: ", ste) ;
    }

    return 1 ;
}

/** copy the source frontend directory to livestate */
static int sanitize_copy_source(resolve_service_t *res)
{
    log_flow() ;

    char *home = res->sa.s + res->path.servicedir ;
    char *live = res->sa.s + res->live.servicedir ;
    char sym[strlen(live) + SS_RESOLVE_LEN + 1] ;
    char dst[strlen(home) + SS_RESOLVE_LEN + 1] ;

    if (!sanitize_livestate_directory(res))
        return 0 ;

    log_trace("copy: ", home, " to: ", live) ;
    if (!hiercopy(home, live))
        log_warnusys_return(LOG_EXIT_ZERO, "copy: ", home, " to: ", live) ;

    auto_strings(sym, live, SS_RESOLVE) ;
    auto_strings(dst, home, SS_RESOLVE) ;

    log_trace("remove directory: ", sym) ;
    if (!dir_rm_rf(sym))
        log_warnusys_return(LOG_EXIT_ZERO, "remove live directory: ", sym) ;

    log_trace("symlink: ", sym, " to: ", dst) ;
    if (!atomic_symlink(dst, sym, "livestate"))
       log_warnusys_return(LOG_EXIT_ZERO, "symlink: ", sym, " to: ", dst) ;

    return 1 ;
}

int sanitize_livestate(resolve_service_t *res, ss_state_t *sta)
{
    log_flow() ;

    int r ;

    r = access(res->sa.s + res->live.servicedir, F_OK) ;
    if (r == -1) {

        if (!sanitize_copy_source(res))
            return 0 ;

    } else if (sta->tounsupervise == STATE_FLAGS_TRUE) {

        log_trace("remove directory: ", res->sa.s + res->live.servicedir) ;
        if (!dir_rm_rf(res->sa.s + res->live.servicedir))
            log_warnusys_return(LOG_EXIT_ZERO, "remove live directory: ", res->sa.s + res->live.servicedir) ;
    }
    return 1 ;
}
